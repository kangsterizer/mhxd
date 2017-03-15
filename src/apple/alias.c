/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"            /* C preprocessor macros */
#endif
#include "api.h"               /* CoreServices */
#include "alias.h"             /* local header */
#include "unicode.h"           /* UTF-8 to UTF-16 conversion */

/* system includes */
#include <sys/types.h>         /* types */
#include <string.h>            /* memcpy */
#include <unistd.h>            /* error macros */
#include <stdio.h>             /* snprintf */
#include <stdlib.h>            /* free */
#include <errno.h>             /* errno */

static inline char *
suffix (const char *path)
{
	char const *p = path + strlen(path);

	while (p > path) {
		p--;
		if (*p == '.')
			return (char *)p;
		if (*p == '/')
			return 0;
	}

	return 0;
}

char* resolve_alias (const char *path, char *resolved_path);

/* functions */

/* alias
   -----
   Will create a Macintosh HFS  specific  alias at the path
   pointed to by dest.  The  alias  will  point to the file
   located at target. The target must exist.

   The return value is zero upon success.  If unsuccessful,
   a  non-zero value will be  returned and both  errno  and
   mac_errno should be consulted.
*/
int alias (const char *target, const char *dest)
{
   char *rtarget;
   char *destparentdir, *destname;
#ifdef HAVE_CORESERVICES
   char *targetsuffix;
   UniChar *udestname;
   FSRef fstarget, fsdest, fsnewfile;
   FSCatalogInfo fscdestinfo, fsctargetinfo;
   FileInfo destinfo, targetinfo;
   AliasHandle aliasrec;
   u_int32_t infomap = 0;
   int resfrk;
   u_int8_t targetisdir = 0;
   int mac_errno;
#endif
   u_int8_t destisdir = 0;
   size_t len = 0;

#ifndef HAVE_CORESERVICES
   /* this function is for creating macintosh aliases.
      if we don't have the CoreServices API it's impossible,
      so why fool ourselves, let's just return error now
      and save the cpu cycles for something else */

   errno = ENOTSUP;
   return -1;
#endif

   /* As of this writing,  this  function is not  complete.
      Things left to  complete are to  check the  target to
      see if it has a custom icon (if so,  copy the  custom
      to the alias file so that they appear the same. Also,
      copy custom icons of bundles to the alias.

      Another very  important thing left to  complete is to
      retire the process of making the alias  manually to a
      last resort if we cannot create the  alias by sending
      an apple event to the finder.
   */

   /* check for null parameters and return accordingly */
   if (!target || !dest) {
      errno = EINVAL;
      return -1;
   }

#ifdef HAVE_CORESERVICES
   memset(&fstarget, 0, sizeof(FSRef));
   memset(&fsdest, 0, sizeof(FSRef));
   memset(&fsnewfile, 0, sizeof(FSRef));
   memset(&fscdestinfo, 0, sizeof(FSCatalogInfo));
   memset(&fsctargetinfo, 0, sizeof(FSCatalogInfo));

   rtarget = (char *)malloc(PATH_MAX);
   if (!rtarget) return -1; /* ENOMEM */

   if (!resolve_alias(target, rtarget)) {
      free(rtarget);
      return -1;
   }

   /* convert the target path into an FSRef */

   mac_errno = FSPathMakeRef(rtarget, &fstarget, &targetisdir);
   if (mac_errno) {
      free(rtarget);
      return mac_errno;
   }

   /* convert the destination into an FSRef so that we can test if it exists
      and if it is a directory (in which we should save the new alias file
      inside the directory specified) */
   mac_errno = FSPathMakeRef(dest, &fsdest, &destisdir);
   /* if the file does not exist, an error code is returned */
#if 0
   if (mac_errno) {
      free(rtarget);
      return mac_errno;
   }
#endif

#endif

   /* if the destination is not a directory, we'll disassemble the path we
      were given for the destination into a parent directory and a name */
   if (!destisdir) {

      /* obtain the name of the destination file */
      len = strlen(dest);
      destname = (char *)dest + len;
      while (destname > dest && *(destname - 1) != '/') destname--, len--;

      /* obtain the parent directory of the destination file */
      destparentdir = (char *)malloc(len);
      snprintf(destparentdir, len ? len + 1 : 3, "%s", len ? dest : "./");

   } else {

      /* the parent directory should be the destination we were passed */
      destparentdir = (char *)dest;

      /* and the name should be the real name of the target */
      destname = (char *)rtarget + strlen(rtarget);
      while (destname > rtarget && *(destname - 1) != '/') destname--;

   }

#ifdef HAVE_CORESERVICES

   /* attempt to convert the parent directory into an FSRef */
   mac_errno = FSPathMakeRef(destparentdir, &fsdest, 0);
   if (destparentdir != dest)
      free(destparentdir);
   if (mac_errno) {
      free(rtarget);
      return mac_errno;
   }

   /* infomap is an integer telling FSCreateResFile which settings from
      the FSCatalogInfo paramater you want to set */
   infomap |= kFSCatInfoFinderInfo;

   /* if the target is not a directory, get its file type/creator */
   if (!targetisdir) {

      /* get the target files information */
      mac_errno = FSGetCatalogInfo(&fstarget, infomap, &fsctargetinfo, 0,0,0);
      if (mac_errno) {
         free(rtarget);
         return mac_errno;
      }

      /* I don't know why Apple did not declare the structure member
         'finderInfo' as an FileInfo type (it is an array of 16 single bytes).
         This makes it impossible to address elements of the finderInfo
         directly. So, we move it into an FileInfo data type we allocated */
      memmove(&targetinfo, fsctargetinfo.finderInfo, sizeof(FileInfo));

      /* this is not really necessary for the creation of the file, since
         a null type or creator will be transposed to '????', but should any
         functions try to display the type/creator, it is much friendlier to
         display '????' than an empty string (which hardcore mac people will
         understand better */
      if (!targetinfo.fileType) {
         targetinfo.fileType = '\?\?\?\?';
         targetinfo.fileCreator = '\?\?\?\?';
      }

   } else {

      /* folders natively return null as their file type and creator but to
         make an alias appear as a folder, we use the following settings */
      targetinfo.fileType = 'fldr';
      targetinfo.fileCreator = 'MACS';

      /* there is one exception, when the folder ends in .app it is an
         application package and should get the following settings */
      targetsuffix = suffix(rtarget);
      if (targetsuffix && !strcmp(targetsuffix, "app")) {
         targetinfo.fileType = 'fapa';
         targetinfo.fileCreator = '\?\?\?\?';
         *--targetsuffix = '\0'; /* this will cut off the .app from the name */
      }

   }

   memset(&destinfo, 0, sizeof(FileInfo));
   /* set the file type, creator, and alias flag for the file */
   destinfo.fileType    = targetinfo.fileType;
   destinfo.fileCreator = targetinfo.fileCreator;
   destinfo.finderFlags = kIsAlias;
   memcpy(fscdestinfo.finderInfo, &destinfo, sizeof(FileInfo));

   udestname = (UniChar *)malloc(PATH_MAX);
   mac_errno = utf8to16(destname, udestname, PATH_MAX, &len);
   free(rtarget);
   if (mac_errno) {
      free(udestname);
      return mac_errno;
   }

   /* attempt to create the resource file (fails if it already exists) */
   FSCreateResFile(&fsdest, len, udestname, infomap, &fscdestinfo, &fsnewfile, 0);

   free(udestname);

   /* test for error creating the file */
   mac_errno = ResError();
   if (mac_errno) {
      return mac_errno;
   }

   /* attempt to open the resource fork of the file now */
   resfrk = FSOpenResFile(&fsnewfile, fsRdWrPerm);

   /* test for errors */
   mac_errno = ResError();
   if (mac_errno) {
      return mac_errno;
   }

   /* attempt to create an alias record to save in the resource file */
   mac_errno = FSNewAlias(0, &fstarget, &aliasrec);
   if (mac_errno) {
      CloseResFile(resfrk);
      return -1;
   }

   /* add the alias record to the resource fork */
   AddResource((Handle)aliasrec, rAliasType, 0, 0);

   /* test for errors */
   mac_errno = ResError();
   if (mac_errno) {
      CloseResFile(resfrk);
      return -1;
   }

   /* write the resource fork data */
   WriteResource((Handle)aliasrec);

   /* test for errors */
   mac_errno = ResError();
   if (mac_errno) {
      CloseResFile(resfrk);
      return -1;
   }

   /* clean up */
   CloseResFile(resfrk);

#endif /* HAVE_CORESERVICES */

   /* return success */
   return 0;

}



/* is_alias
   --------
   Sets the second parameter to true if the file pointed to
   by path is an alias or symbolic link, otherwise sets the
   value to false. Return status the the mac error code.
*/
int is_alias (const char *path, int *is_link)
{
#ifdef HAVE_CORESERVICES
   FSRef fspath;
   Boolean isdir = 0, isalias = 0;
#else
   int isalias = 0;
#endif
   int mac_errno;

#ifdef HAVE_CORESERVICES

   /* attempt to convert the target path into an FSRef */
   mac_errno = FSPathMakeRef(path, &fspath, 0);
   if (mac_errno) return mac_errno;

   /* check if the file is actually an alias */
   mac_errno = FSIsAliasFile(&fspath, &isalias, &isdir);
   if (mac_errno) return mac_errno;

#endif

   *is_link = isalias;

   return 0;
}



/* resolve_alias
   -------------
   Resolves the Macintosh alias file at the path pointed to
   by path and stores the resolved path in resolved_path.

   The return value is a  pointer to the resolved path upon
   success.  On  error the  return  value is  zero and both
   errno and mac_errno should be consulted.
*/
char* resolve_alias (const char *path, char *resolved_path)
{
#ifdef HAVE_CORESERVICES
   FSRef fspath;
   Boolean isdir = 0, isalias = 0;
#endif
   int mac_errno;

#ifndef HAVE_CORESERVICES
   /* this function is for resolving macintosh aliases.
      if we don't have the CoreServices API it's impossible,
      so why fool ourselves, let's just return error now
      and save the cpu cycles for something else */

   errno = ENOTSUP;
   return 0;
#endif

   if (!path) return 0;

   /* if the second parameter is null, assume caller wants it allocated */
   if (!resolved_path) {
      resolved_path = (char *)malloc(PATH_MAX + 1);
      if (!resolved_path) return 0;
   }

#ifdef HAVE_CORESERVICES

   /* attempt to convert the target path into an FSRef */
   mac_errno = FSPathMakeRef(path, &fspath, 0);
   if (mac_errno) return 0;

   /* check if the file is actually an alias */
   mac_errno = FSIsAliasFile(&fspath, &isalias, &isdir);
   if (mac_errno) return 0;

   /* resolve the path completely */
   mac_errno = FSResolveAliasFile(&fspath, true, &isdir, &isalias);
   if (mac_errno) return 0;

   /* obtain the resolved path */
   mac_errno = FSRefMakePath(&fspath, resolved_path, PATH_MAX);
   if (mac_errno) return 0;

#endif

   /* return the resolved path upon success */
   return resolved_path;
}

char* resolve_alias_path (const char *path, char *resolved_path)
{
   char *p = (char *)path;
   char temp[PATH_MAX], newpath[PATH_MAX];
   u_int32_t i;

   if (!path) return 0; /* null pointer */

   /* return if the path is too many characters */
   if (strlen(path) > PATH_MAX) {
      errno = ENAMETOOLONG; /* set errno appropriately */
      return 0;
   }

   newpath[0] = 0;

   /* assume cwd if an empty path is passed */
   if (!*path) {
      strcpy(temp, "./");
      if (!resolve_alias(temp, newpath))
         return 0;
   } else if (*path == '/') {
      strcpy(newpath, "/");
      while (*p == '/') p++;
   }

   while (*p) {
      strcpy(temp, newpath);
      i = strlen(temp);
      if (*newpath) temp[i++] = '/';
      while (*p && *p != '/')
         temp[i++] = *p++;
      temp[i] = 0;
      if (*p == '/')
         while (*p == '/') p++;
      if (!resolve_alias(temp, newpath))
         return 0;
   }

   if (!resolved_path) {
      resolved_path = (char *)malloc(PATH_MAX);
      if (!resolved_path) return 0;
   }

   strcpy(resolved_path, newpath);

   return resolved_path;
}
