/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*---------------------------------------------------------------------------

  izzip_example.c

  Example main program illustrating how to use the libizzip object
  library (Unix: libizzip.a, VMS: LIBIZZIP.OLB).

  To build the library (from the top Zip source directory):

    make unix/Makefile BINS=L generic

  See INSTALL and the comments in Makefile for more detailed instructions.

  Basic build procedure, Unix:

    cc izzip_example.c -DZIPLIB -IZip_Source_Dir -o izzip_example \
     -LZip_Object_Dir -lizzip

  (On Unix, the default Zip_Object_Dir is the same as the Zip_Source_Dir.)

  For example:

    cc izzip_example.c -o izzip_example libizzip.a -DZIPLIB

  Basic build procedure, VMS:

    cc izzip_example.c /define = ZIPLIB=1 -
     /include = (Zip_Source_Dir, Zip_Source_Vms_Dir)
    link izzip_example.obj, Zip_Object_Dir:libizzip.olb /library

  Now that bzip2 is included in the libizzip combined library build, no
  additional library linkages should be required.  Be sure the build of
  libizzip.a includes any compression and encryption capabilities needed.
  If an external/system library is used to build libizzip, it may be
  necessary to link explicitly to that external library when building
  izzip_example.

  On VMS, a link options file (LIB_IZZIP.OPT) is generated along with
  the object library (in the same directory), and it can be used to
  simplify the LINK command.  (LIB_IZZIP.OPT contains comments showing
  how to define the logical names which it uses.)

    define LIB_IZZIP Dir        ! See comments in LIB_IZZIP.OPT.
    define LIB_other Dir        ! See comments in LIB_IZZIP.OPT.
    link izzip_example.obj, Zip_Object_Dir:lib_izzip.opt /options

  See ReadLibDll.txt for more on the updated Zip 3.1 LIB/DLL interface.

  Note that API_FILESIZE_T is usually 64 bits (unsigned long long), but
  is 32 bits (unsigned long) on VAX VMS.

  Original example by SMS, since modified.

  Last updated 2015-09-21.

  ---------------------------------------------------------------------------*/

#include "api.h"                /* Zip specifics. */

#include <stdio.h>


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
     char *info                          additional info (e.g. encryption)
     long percent                        compression percent result

   MyZipService is called after an entry is processed.  If MyZipService()
   returns non-zero, operation will terminate.  Return 0 to continue.
 */

int GetVersionInfo(unsigned char *maj,
                   unsigned char *min,
                   unsigned char *patch);

char *argv_to_commandline(int argc, char *argv[]);

/* -------------------------------------------------------------------- */
/*
 * main(): Example main program.
 */

int main(int argc, char **argv)
{
    int ret;
#ifdef __VMS
    int vsts;
#endif
    char CurrentDir[1024];
    char ProgressChunkSize[10];
    char *commandline = NULL;
    unsigned char maj = 0;
    unsigned char min = 0;
    unsigned char patch = 0;
    ZIPUSERFUNCTIONS ZipUserFunctions;

    if (argc < 2)
    {
      printf("usage: izzip_example [zip command arguments]\n");
      return 90;           /* Exits if not proper number of arguments */
    }

    fprintf(stderr, "-------------------------\n");
    fprintf(stderr, "izzip_example\n");

    /* Any unused callbacks must be set to NULL.  Leave this
       block as is and add functions as needed below. */
    ZipUserFunctions.print = NULL;         /* gets most output to stdout */
    ZipUserFunctions.ecomment = NULL;      /* called to set entry comment */
    ZipUserFunctions.acomment = NULL;      /* called to set archive comment */
    ZipUserFunctions.password = NULL;      /* called to get password */
    ZipUserFunctions.split = NULL;         /* NOT IMPLEMENTED - DON'T USE */
    ZipUserFunctions.service = NULL;       /* called each entry */
    ZipUserFunctions.service_no_int64 = NULL; /* 32-bit version of service */
    ZipUserFunctions.progress = NULL;      /* provides entry progress */
    ZipUserFunctions.error = NULL;         /* called when warning/error */
    ZipUserFunctions.finish = NULL;        /* called when zip is finished */

    /* Set callbacks for the functions we're using. */
    ZipUserFunctions.print       = MyZipPrint;
    ZipUserFunctions.ecomment    = MyZipEComment;
    ZipUserFunctions.acomment    = MyZipAComment;
    ZipUserFunctions.password    = MyZipPassword;
    ZipUserFunctions.service     = MyZipService;


    /* Using ZpInit and calling zipmain directly is deprecated. */
#if 0
    fprintf(stderr, " Calling ZpInit\n");
    ret = ZpInit( &ZipUserFunctions);
    fprintf( stderr, "  ZipInit() sts = %d (%%x%08x).\n", ret, ret);

    fprintf(stderr, " Calling zipmain\n");
    fprintf(stderr, "-------\n");
    ret = zipmain(argc, argv);
    fprintf(stderr, "-------\n");
#endif

    /* ZpZip will CD to here to make this zip root. */
    strcpy(CurrentDir, ".");

    /* Bytes read between progress report callbacks.  This is only used
       by the progress() callback.  If you don't use that callback, this
       parameter can be set to NULL.  However, no harm always setting it. */
    strcpy(ProgressChunkSize, "100k");


    fprintf(stderr, "Getting version information:\n");
    fprintf(stderr, "-----------\n");
  
    ret = GetVersionInfo(&maj, &min, &patch);
  
    fprintf(stderr, "-----------\n");

    if (maj != 3 && min != 1) {
      fprintf(stderr, "LIB version is:  %d.%d.%d\n", maj, min, patch);
      fprintf(stderr, "This example program requires version 3.1.x\n");
      return 91;
    }

    /* This converts the rest of the command line arguments to a command
       line string for Zip to execute.  You can replace this with the
       command line string you want Zip to execute. */
    commandline = argv_to_commandline(argc, argv);

    fprintf(stderr, "Command line:  zip %s\n", commandline);

    fprintf(stderr, " Calling ZpZip\n");
    fprintf(stderr, "-------\n");
    ret = ZpZip(commandline, CurrentDir, &ZipUserFunctions, ProgressChunkSize);
    fprintf(stderr, "-------\n");

    fprintf(stderr, "  Zip returned = %d (%%x%08x).\n", ret, ret);
#ifdef __VMS
    vsts = vms_status(ret);
    fprintf( stderr, "  VMS sts = %d (%%x%08x).\n", vsts, vsts);
#endif

    fprintf(stderr, "\n");

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

    fprintf(stderr, ".   Enter the new comment for %s (hit ENTER to keep):\n",
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

    fprintf(stderr, ". Enter the new zip file comment (hit ENTER to keep):\n");

    /* The zip file comment can be multiple lines (with \r\n line ends), but we
       only get one line here. */
    fprintf(stderr, ". Zip file comment: ");
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

    fprintf(stderr, ". Enter the password: ");
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

    fprintf(stderr, ". returning password = \"%s\"\n",
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
 *   method                        compression method used (e.g. "deflate")
 *   info                          additional info (e.g. "AES256" encrypted)
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
 * All Zip output to stdout gets routed to this callback.  If the
 * callback address for this function is NULL, stdout output is
 * discarded.
 */

long MyZipPrint(char *buf, unsigned long size)
{
    fprintf(stderr, "%s", buf);
#if 0
    fprintf(stderr, "ZP(%d bytes): %s", size, buf);
#endif
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


