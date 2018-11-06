/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/* c_lib_ex.c
 *
 * A very simplistic Windows C example of how to use the Zip LIB.
 *
 * This example is very simpler to the Unix/VMS LIB example, izzip_example.c,
 * located in the root directory of this Zip source kit.  Previous versions
 * of this example required the bzip2 library to be built separately and
 * linked into the user application in addition to the Zip LIB.  Now just
 * the Zip static library (zip32_lib.lib or zip32_libd.lib) is sufficient.
 *
 * Notes have been left in the project folders where the library files
 * should be placed.
 *
 * This example now includes callback functions for comments and passwords,
 * as well as an example of the Service callback.  See the comments for each
 * function below for more details.
 *
 * It is important that the matching version of the library be linked to
 * when building c_lib_ex, i.e. if c_lib_ex is being build as a DEBUG
 * application, the DEBUG version of the library (zip32_libd.lib) should be
 * used.  If the RELEASE version of this application is being build, the
 * RELEASE version of the library (zip32_lib.lib) should be used.  Mixing
 * DEBUG and RELEASE versions may result in unexpected crashes.
 *
 * A typical test command line might be:
 *   c_lib_ex testarchive test.txt -ecz
 * which would add/update test.txt (you may need to create this input file)
 * in testarchive.zip, using the password callback to get the password, the
 * entry comment callback to get the entry comment for test.txt, and the
 * archive comment callback to get the archive comment.  If the Service
 * callback is enabled, the LIB will call that callback once test.txt is
 * processed to return the status as well as the compression method used
 * and the compression achieved.
 *
 * Note that this example DOES NOT handle Unicode on the command line.
 * Though Zip does on Windows, this example does not read the wide character
 * command line, and so any Unicode on the command line is lost.  The
 * command line passed to Zip via ZpZip() can include UTF-8.  So if this
 * application read the wide command line and converted the args to UTF-8,
 * then passed that UTF-8 to Zip, then it could handle Unicode command lines.
 *
 * We plan to add callback examples for the remaining callbacks in the next
 * beta or by release.
 *
 * A developer of course can update the callback code to do what's needed.
 * Other examples in the Zip source kit (such as the graphical Visual Basic
 * examples) demonstrate more complex solutions.
 *
 * 2014-09-17 EG
 * Last updated 2015-09-21 EG
 */


#if 0
#ifndef WIN32
#  define WIN32
#endif
#define API
#endif

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
#ifdef WIN32
#include <commctrl.h>
#include <winver.h>
#else
#include <ver.h>
#endif
#endif

#if 0
extern char *szCommentBuf;
#endif

/* ------------- */
/* LIB callbacks */

long MyZipPrint(char *buf,               /* in - what was printed */
                unsigned long size);     /* in - length of buf */

long MyZipEComment(char *oldcomment,     /* in - old comment */
                   char *filename,       /* in - local char file name */
                   char *ufilename,      /* in - UTF-8 file name */
                   long maxcommentsize,  /* in - max new comment size */
                   long *newcommentsize, /* out - size of new comment */
                   char *newcomment);    /* out - new comment */

long MyZipAComment(char *oldcomment,     /* in - old comment */
                   long maxcommentsize,  /* in - max new comment size */
                   long *newcommentsize, /* out - new comment size */
                   char *newcomment);    /* out - new comment */

long MyZipPassword(long bufsize,         /* in - max password size */
                   char *prompt,         /* in - prompt ("enter"/"verify") */
                   char *password);      /* out - password */

long MyZipService(char *file_name,
                  char *unicode_file_name,
                  char *uncompressed_size_string,
                  char *compressed_size_string,
                  API_FILESIZE_T uncompressed_size,
                  API_FILESIZE_T compressed_size,
                  char *action,
                  char *method,
                  char *info,
                  long percent);

/* MyZipService (all in parameters, nothing returned via parameters):
     char *file_name                     current file being processed
     char *unicode_file_name             UTF-8 name of file
     char *uncompressed_size_string      string usize (e.g. "1.2m" = 1.2 MiB)
     char *compressed_size_string        string csize
     API_FILESIZE_T uncompressed_size    int usize
     API_FILESIZE_T compressed_size      int csize
     char *action                        what was done (e.g. "UPDATE")
     char *method                        method used (e.g. "Deflate")
     char *info                          additional info (e.g. "AES256")
     long percent                        compression percent result

   MyZipService is called after an entry is processed.  If MyZipService()
   returns non-zero, operation will terminate.  Return 0 to continue.
 */

/* ------------- */

int GetVersionInfo(unsigned char *maj,
                   unsigned char *min,
                   unsigned char *patch);

char *argv_to_commandline(int argc, char *argv[]);



/****************************************************************************

    Main()

    Returns 90 + something if our error, else returns what Zip returns.

****************************************************************************/
int main(int argc, char **argv)
{
  int ret;
  char *commandline = NULL;
  char *currentdir = ".";
  char *progress_size = "100m";
  unsigned char maj = 0;
  unsigned char min = 0;
  unsigned char patch = 0;
  ZIPUSERFUNCTIONS ZipUserFunctions, *lpZipUserFunctions;

  if (argc < 2)
  {
    printf("usage: c_lib_ex [zip command arguments]\n");
    return 90;           /* Exits if not proper number of arguments */
  }

  fprintf(stderr, "\n");
  fprintf(stderr, "c_lib_ex example\n");
  fprintf(stderr, "\n");


  fprintf(stderr, "Getting version information:\n");
  fprintf(stderr, "-----------\n");
  
  ret = GetVersionInfo(&maj, &min, &patch);
  
  fprintf(stderr, "-----------\n");

  if (maj != 3 && min != 1) {
    fprintf(stderr, "LIB version is:  %d.%d.%d\n", maj, min, patch);
    fprintf(stderr, "This example program requires version 3.1.x\n");
    return 91;
  }


  commandline = argv_to_commandline(0, argv);
  if (commandline == NULL)
  {
    fprintf(stderr, "could not allocate commandline\n");
    return 92;
  }

  fprintf(stderr, "\n");
  fprintf(stderr, "commandline:  '%s'\n", commandline);
  fprintf(stderr, "\n");
  
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
  ZipUserFunctions.print        = MyZipPrint;
  ZipUserFunctions.ecomment     = MyZipEComment;
  ZipUserFunctions.acomment     = MyZipAComment;
  ZipUserFunctions.password     = MyZipPassword;
  ZipUserFunctions.service      = MyZipService;

  fprintf(stderr, "Calling Zip...\n");
  fprintf(stderr, "-----------------------------------------\n");

  ret = ZpZip(commandline, currentdir, lpZipUserFunctions, progress_size);

  fprintf(stderr, "-----------------------------------------\n");

  fprintf(stderr, "Zip returned:  %d\n", ret);

  return ret;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipEComment(): Entry comment call-back function.
 *
 * Maximum comment length: 32765.  (The actual maximum length is dependent
 *                                  on various factors, but this should be
 *                                  safe.  zip.h defines MAX_COM_LEN for
 *                                  this.)
 *
 * Caller provides buffer (newcomment) to write comment to.
 *
 * Return 0 on fail (don't update comment), 1 on success.
 *
 * Update this function to meet the needs of your application.
 */

long MyZipEComment(char *oldcomment,
                   char *filename,
                   char *ufilename,
                   long maxcommentsize,
                   long *newcommentsize,
                   char *newcomment)
{
    int oldcommentsize = 0;
    char *comment = NULL;
    char newcommentstring[MAX_COM_LEN];

    if (oldcomment)
      oldcommentsize = strlen(oldcomment);

    fprintf(stderr, ". EComment callback:\n");
    fprintf(stderr, ".   old comment for %s (%d bytes):\n",
            filename, oldcommentsize);
    fprintf(stderr, ".     \"%s\"\n",
            oldcomment);

    /* here is where you would update the comment */

    fprintf(stderr, ". - Enter the new comment for %s (hit ENTER to keep):\n",
            filename);
    fprintf(stderr, ".   Entry comment: ");
    fflush(stderr);

    comment = fgets(newcommentstring, MAX_COM_LEN, stdin);
    if (comment = NULL) {
      return 0;
    }

    *newcommentsize = strlen(newcommentstring);

    if (*newcommentsize == 0 ||
        (*newcommentsize == 1 && newcommentstring[0] == '\n')) {
      fprintf(stderr, ".   keeping: %s\n", oldcomment);
      return 0;
    }

    if (*newcommentsize > maxcommentsize - 1)
      *newcommentsize = maxcommentsize - 1;

    strncpy(newcomment, newcommentstring, *newcommentsize);
    newcomment[*newcommentsize] = '\0';

    if (newcomment[*newcommentsize - 1] == '\n')
      newcomment[*newcommentsize - 1] = '\0';

    fprintf(stderr, ".   new comment for %s (%d bytes):\n",
            filename, *newcommentsize);
    fprintf(stderr, ".     \"%s\"\n",
            newcomment);
    *newcommentsize = strlen(newcomment);

    return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipAComment(): Archive comment call-back function.
 *
 * Maximum comment length: 32765.  (The actual maximum length is dependent
 *                                  on various factors, but this should
 *                                  be safe.  zip.h defines MAX_COM_LEN
 *                                  for this.)
 *
 * Caller provides buffer (newcomment) to write comment to.
 *
 * Return 0 on fail (don't update comment), 1 on success.
 *
 * Update this function to meet the needs of your application.
 */

long MyZipAComment(char *oldcomment,
                   long maxcommentsize,
                   long *newcommentsize,
                   char *newcomment)
{
    int oldcommentsize = 0;
    char *comment = NULL;
    char newcommentstring[MAX_COM_LEN];

    if (oldcomment)
      oldcommentsize = strlen(oldcomment);

    fprintf(stderr, ". AComment callback:\n");
    fprintf(stderr, ".   old archive comment (%d bytes):\n",
            oldcommentsize);
    fprintf(stderr, ".     \"%s\"\n",
            oldcomment);

    /* here is where you update the comment */

    fprintf(stderr, ". - Enter the new zip file comment (hit ENTER to keep):\n");

    /* The zip file comment can be multiple lines (with \r\n line ends), but we
       only get one line here. */
    fprintf(stderr, ".   Zip file comment: ");
    fflush(stderr);
    comment = fgets(newcommentstring, MAX_COM_LEN, stdin);
    if (comment = NULL) {
      return 0;
    }

    *newcommentsize = strlen(newcommentstring);
    if (*newcommentsize > maxcommentsize - 1)
      *newcommentsize = maxcommentsize - 1;

    if (*newcommentsize == 0 ||
        (*newcommentsize == 1 && newcommentstring[0] == '\n')) {
      fprintf(stderr, ".   keeping: %s\n", oldcomment);
      return 0;
    }

    strncpy(newcomment, newcommentstring, *newcommentsize);
    newcomment[*newcommentsize] = '\0';

    if (newcomment[*newcommentsize - 1] == '\n')
      newcomment[*newcommentsize - 1] = '\0';

    fprintf(stderr, ".   new archive comment (%d bytes):\n",
            *newcommentsize);
    fprintf(stderr, ".     \"%s\"\n",
            newcomment);
    *newcommentsize = strlen(newcomment);

    return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipPassword(): Encryption password call-back function.
 *
 * bufsize  - max size of password, including terminating NULL
 * prompt   - either the "password" or "verify password" prompt text
 * password - caller provides buffer (password) to write password to
 *
 * Caller provides buffer (password) to write password to.
 *
 * Return 0 on fail, 1 on success.
 *
 * Modify as needed for your application.
 */

long MyZipPassword(long bufsize,
                   char *prompt,
                   char *password)
{
    char *passwd = NULL;
    int passwordlen;
    char newpassword[MAX_PASSWORD_LEN];

    fprintf(stderr, ". Password callback:\n");
    fprintf(stderr, ".   bufsize            = %d\n", bufsize);
    fprintf(stderr, ".   prompt             = '%s'\n", prompt);

    fprintf(stderr, ". - Enter the password: ");
    fflush(stderr);

    passwd = fgets(newpassword, MAX_PASSWORD_LEN, stdin);
    if (passwd = NULL) {
      return 0;
    }

    passwordlen = strlen(newpassword);
    if (passwordlen > bufsize - 1)
      passwordlen = bufsize - 1;

    if (newpassword[passwordlen - 1] == '\n')
      newpassword[passwordlen - 1] = '\0';

    strncpy(password, newpassword, passwordlen);
    password[passwordlen] = '\0';

    fprintf(stderr, ".   returning password = \"%s\"\n",
            newpassword);

    return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipService(): Service call-back function.
 *
 * MyZipService is called after each entry is processed to provide a status
 * update.  See also the Progress call-back function, which provides updates
 * during processing as well as after an entry is processed and allows
 * terminating the processing of a large file.
 *
 * If MyZipService() returns non-zero, operation will terminate.  Return 0 to
 * continue.
 *
 *   file_name                     current file being processed
 *   unicode_file_name             UTF-8 name of file
 *   uncompressed_size_string      string usize (e.g. "1.2m" = 1.2 MiB)
 *   compressed_size_string        string csize
 *   uncompressed_size             int usize
 *   compressed_size               int csize
 *   action                        what was done (e.g. "UPDATE", "DELETE")
 *   method                        method used (e.g. "Deflate")
 *   info                          additional info (e.g. "AES256")
 *   percent                       compression percent result
 *
 * This example assumes file sizes no larger than an unsigned long (2 TiB).
 *
 * Return 0 to continue.  Return 1 to abort the Zip operation.
 *
 * Modify as needed for your application.
 */

long MyZipService(char *file_name,
                  char *unicode_file_name,
                  char *uncompressed_size_string,
                  char *compressed_size_string,
                  API_FILESIZE_T uncompressed_size,
                  API_FILESIZE_T compressed_size,
                  char *action,
                  char *method,
                  char *info,
                  long percent)
{
  /* Assume that any files tested by the example are smaller than 2 TB. */
  unsigned long us = (unsigned long)uncompressed_size;
  unsigned long cs = (unsigned long)compressed_size;

  fprintf(stderr, ". Service callback:\n");
  fprintf(stderr, ".   File name:                 %s\n", file_name);
  fprintf(stderr, ".   UTF-8 File name:           %s\n", unicode_file_name);
  fprintf(stderr, ".   Uncompressed size string:  %s\n", uncompressed_size_string);
  fprintf(stderr, ".   Compressed size string:    %s\n", compressed_size_string);
  fprintf(stderr, ".   Uncompressed size:         %ld\n", us);
  fprintf(stderr, ".   Compressed size:           %ld\n", cs);
  fprintf(stderr, ".   Action:                    %s\n", action);
  fprintf(stderr, ".   Method:                    %s\n", method);
  fprintf(stderr, ".   Info:                      %s\n", info);
  fprintf(stderr, ".   Percent:                   %ld\n", percent);

  /* Return 0 to continue.  Set this to 1 to abort the operation. */
  return 0;
}

/* -------------------------------------------------------------------- */
/*
 * MyZipPrint(): Message output call-back function.
 *
 * Any message Zip would normally display is instead received by this
 * function.
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
int GetVersionInfo(unsigned char *maj,
                   unsigned char *min,
                   unsigned char *patch)
{
  ZpVer zip_ver;

  char zipversnum[100];
  char libdll_interface_vers_num[100];
  char c;
  int i;
  int j;
  int k = 0;
  char features[4000];

  /* Allocate memory for each thing to be returned. */

  if ((zip_ver.BetaLevel = malloc(10)) == NULL)
  {
    fprintf(stderr, "Could not allocate BetaLevel\n");
    return 93;
  }
  if ((zip_ver.Version = malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate Version\n");
    return 94;
  }
  if ((zip_ver.RevDate = malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate RevDate\n");
    return 95;
  }
  if ((zip_ver.RevYMD = malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate RevYMD\n");
    return 96;
  }
  if ((zip_ver.zlib_Version = malloc(10)) == NULL)
  {
    fprintf(stderr, "Could not allocate zlib_Version\n");
    return 97;
  }
  if ((zip_ver.szFeatures = malloc(4000)) == NULL)
  {
    fprintf(stderr, "Could not allocate szFeatures\n");
    return 98;
  }
  ZpVersion(&zip_ver);

  features[0] = '\0';
  for (i = 0, j = 0; c = zip_ver.szFeatures[i]; i++)
  {
    features[j++] = c;
    k++;
    if (c == ';')
    {
      features[j++] = ' ';
      if (k > 60) {
        features[j++] = '\n';
        for (k = 0; k < 21; k++) {
          features[j++] = ' ';
        }
        k = 0;
      }
    }
  }
  features[j] = '\0';

  *maj = zip_ver.libdll_interface.major;
  *min = zip_ver.libdll_interface.minor;
  *patch = zip_ver.libdll_interface.patchlevel;

  sprintf(zipversnum, "%d.%d.%d",
          zip_ver.zip.major,
          zip_ver.zip.minor,
          zip_ver.zip.patchlevel);
  sprintf(libdll_interface_vers_num, "%d.%d.%d",
          *maj,
          *min,
          *patch);
  fprintf(stderr, "  Zip version:       %s\n", zip_ver.Version);
  fprintf(stderr, "  Zip version num:   %s %s\n", zipversnum,
                                                   zip_ver.BetaLevel);
  fprintf(stderr, "  Zip rev date:      %s\n", zip_ver.RevDate);
  fprintf(stderr, "  Zip rev YMD:       %s\n", zip_ver.RevYMD);
  fprintf(stderr, "  LIB/DLL interface: %s\n", libdll_interface_vers_num);
  fprintf(stderr, "  Feature list:      %s\n", features);

  free(zip_ver.BetaLevel);
  free(zip_ver.Version);
  free(zip_ver.RevDate);
  free(zip_ver.zlib_Version);

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
