/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"             /* C preprocessor macros */
#endif
#include "files.h"              /* local header */

/* system includes */
#include <sys/types.h>          /* types */
#include <errno.h>              /* errno, EIO */

/* functions */

int mac_get_type (const char *path, char *type, char *creator)
{
#ifdef HAVE_CORESERVICES
   FSRef fspath;
   FSCatalogInfo fscinfo;
   FileInfo info;
   u_int32_t infomap = 0;
   u_int8_t isdir = 0;
   int mac_errno;
#endif

#ifndef HAVE_CORESERVICES
   /* this function is for getting HFS file types. 
      if we don't have the CoreServices API it's impossible,
      so why fool ourselves, let's just return error now
      and save the cpu cycles for something else */

   errno = ENOTSUP;
   return -1;
#endif

#ifdef HAVE_CORESERVICES

   /* convert the path into an FSRef */

   mac_errno = FSPathMakeRef(path, &fspath, &isdir);
   if (mac_errno) return mac_errno;

   /* infomap is an integer telling FSGetCatalogInfo which settings
      from the FSCatalogInfo parameter you want to get */
   infomap |= kFSCatInfoFinderInfo;

   /* get the File Catalog Information */

   mac_errno = FSGetCatalogInfo(&fspath, infomap, &fscinfo, 0,0,0);
   if (mac_errno) return mac_errno;

   /* I don't know why Apple did not declare the structure member
         'finderInfo' as an FileInfo type (it is an array of 16 single bytes).
         This makes it impossible to address elements of the finderInfo
         directly. So, we move it into an FileInfo data type we allocated */
   memmove(&info, fscinfo.finderInfo, sizeof(FileInfo));

   /* copy the file type information */
   if (!isdir) {
      if (type) {
         if (!info.fileType)
            strcpy(type, "????");
         else {
            memcpy(type, &(info.fileType), 4);
            type[4] = 0;
         }
      }
      if (creator) {
         if (!info.fileCreator)
            strcpy(creator, "????");
         else {
            memcpy(creator, &(info.fileCreator), 4);
            creator[4] = 0;
         }
      }
   } else {
      if (type)
         strcpy(type, "fldr");
      if (creator)
         strcpy(creator, "MACS");
   }

   return 0;

#endif /* HAVE_CORESERVICES */

}



int mac_set_type (const char *path, const char *type, const char *creator)
{
#ifdef HAVE_CORESERVICES
   FSRef fspath;
   FSCatalogInfo fscinfo;
   FileInfo info;
   u_int32_t infomap = 0;
   u_int8_t isdir = 0;
   int mac_errno;
#endif

#ifndef HAVE_CORESERVICES
   /* this function is for setting HFS file types. 
      if we don't have the CoreServices API it's impossible,
      so why fool ourselves, let's just return error now
      and save the cpu cycles for something else */

   errno = ENOTSUP;
   return -1;
#endif

#ifdef HAVE_CORESERVICES

   /* convert the path into an FSRef */

   mac_errno = FSPathMakeRef(path, &fspath, &isdir);
   if (mac_errno) return mac_errno;

   /* infomap is an integer telling FSGetCatalogInfo which settings
      from the FSCatalogInfo parameter you want to get/set */
   infomap |= kFSCatInfoFinderInfo;

   /* get the current File Catalog Information to obtain flags, etc. */
   mac_errno = FSGetCatalogInfo(&fspath, infomap, &fscinfo, 0,0,0);
   if (mac_errno) return mac_errno;

   /* move the data to our FileInfo structure */
   memmove(&info, fscinfo.finderInfo, sizeof(FileInfo));

   /* modify the type/creator of the file information */

   if (type) memcpy(&(info.fileType), type, 4);
   else info.fileType = '\?\?\?\?';
   if (creator) memcpy(&(info.fileCreator), creator, 4);
   else info.fileCreator = '\?\?\?\?';

   /* move the data with modified type/creator back to the FSCatalogInfo */
   memmove(fscinfo.finderInfo, &info, sizeof(FileInfo));

   /* set the File Catalog Information */
   mac_errno = FSSetCatalogInfo(&fspath, infomap, &fscinfo);
   if (mac_errno) return mac_errno;

   return 0;

#endif /* HAVE_CORESERVICES */

}
