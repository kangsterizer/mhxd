/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"            /* C preprocessor macros */
#endif
#include "api.h"               /* CoreServices */
#include "unicode.h"           /* prototype */

/* system includes */
#include <sys/types.h>         /* types */
#include <string.h>            /* memcpy */
#include <unistd.h>            /* error macros */
#include <stdio.h>             /* snprintf */
#include <stdlib.h>            /* free */
#include <errno.h>             /* errno */

#ifdef HAVE_CORESERVICES
/* Static  variables that get  initialized on the first run
   and then re-used every time after that.  These variables
   never change, so by re-using them we increase efficiency
   of functions that require them. */
static TextEncoding kTextEncodingUTF8 = 0;
static UnicodeToTextInfo *UtoTInfo = 0;
static TextToUnicodeInfo *TtoUInfo = 0;
#endif

/* functions */

/* utf8to16
   --------
   This function takes a standard UTF-8 encoded string (the
   standard for POSIX routines) and converts it to a UTF-16
   encoding.

   The return  value is the  number of  bytes used from the
   UTF-8 encoded string if  successful.  -1 is  returned on
   error and both errno and mac_errno should be consulted.

   This routine requires the  Macintosh CoreServices API to
   function.
*/
int utf8to16 (const char *unicode8, u_int16_t *buf, size_t buflen, size_t *lenp)
{
#ifdef HAVE_CORESERVICES
   UnicodeMapping mapinfo;
#endif
   size_t len = 0;
   char pstr[256];
   int mac_errno;

#ifndef HAVE_CORESERVICES
   /* This function requires the Macintosh CoreServices API
      so it would be  silly for us to  continue on if we do
      not have it. */

   errno = ENOTSUP;
   return -1;
#endif

   /* check for sufficient and logical parameters */
   if (!unicode8 || !buf || !buflen || !lenp) {
      errno = EINVAL;
      return -1;
   }

#ifdef HAVE_CORESERVICES

   /* Special thanks goes to catalyst for providing help with the UTF-8 Text
      Encoding routines. I optimized it for speed and ease of use/reading */

   /* pointers to static data are used because we don't want to recreate a
      constant object everytime the function is called. We'll simply declare
      our constant data that never changes (the conversion descriptors) as
      static pointers and allocate/create them on the first run. Then every
      successive call will run faster */

   /* initialize the text encoding if not already done so */
   if (!kTextEncodingUTF8) {
      kTextEncodingUTF8 = CreateTextEncoding(
         kTextEncodingUnicodeV3_0,
         kUnicodeNoSubset,
         kUnicodeUTF8Format);
   }

   /* create the UnicodeToTextInfo object if not initialized already */
   if (!UtoTInfo) {
      /* create a mapping structure for the UnicodeToTextInfo object */
      mapinfo.unicodeEncoding = kTextEncodingUTF8;
      mapinfo.otherEncoding = kTextEncodingMacRoman;
      mapinfo.mappingVersion = kUnicodeUseLatestMapping;

      UtoTInfo = (UnicodeToTextInfo *)malloc(sizeof(UnicodeToTextInfo));
      if (!UtoTInfo) return -1; /* errno is set to ENOMEM */
      mac_errno = CreateUnicodeToTextInfo(&mapinfo, UtoTInfo);
      if (mac_errno) return mac_errno; /* caller should consult mac_errno */
   }

   /* create the TextToUnicodeInfo object if not initialized */
   if (!TtoUInfo) {
      TtoUInfo = (TextToUnicodeInfo *)malloc(sizeof(TextToUnicodeInfo));
      if (!TtoUInfo) return -1; /* errno is set to ENOMEM */
      mac_errno = CreateTextToUnicodeInfoByEncoding(
         kTextEncodingMacRoman, TtoUInfo);
      if (mac_errno) return mac_errno; /* caller should consult mac_errno */
   }

#endif /* HAVE_CORESERVICES */

#ifdef HAVE_CORESERVICES

   /* convert the UTF-8 supplied parameter to a Pascal string in Mac Roman */
   mac_errno = ConvertFromUnicodeToPString(*UtoTInfo,
      strlen(unicode8), (UniChar *)unicode8, pstr);
   if (mac_errno) return mac_errno; /* caller should consult mac_errno */

   /* now convert the Mac Roman encoded Pascal string to UTF-16 */
   mac_errno = ConvertFromPStringToUnicode(*TtoUInfo, pstr, buflen, &len, buf); 
   if (mac_errno) return mac_errno; /* caller should consult mac_errno */

   /* we should really ignore the length attribute modified by the last call
      because it returns the length of the UTF-16 string in bytes when in
      reality we want it's length in characters (number of 16 bit sequences) */

#endif

   /* the first byte of the Pascal string holds the correct length that we
      want to return (so we copy it into len before freeing it) */
   len = pstr[0];

   *lenp = len;

   return 0;

}
