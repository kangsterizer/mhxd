#ifndef HAVE_UNICODE_H
#define HAVE_UNICODE_H

#include <sys/types.h>   /* types such as u_int16_t and size_t */
#if defined(HAVE_CONFIG_H)
#include "config.h"      /* same as above in case not found in system */
#endif

#ifdef __cplusplus
extern "C" {
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
int utf8to16 (const char *, u_int16_t *, size_t, size_t *);



#ifdef __cplusplus
}
#endif

#endif /* HAVE_UNICODE_H */
