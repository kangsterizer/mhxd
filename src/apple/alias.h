#ifndef HAVE_APPLE_ALIAS_H
#define HAVE_APPLE_ALIAS_H

/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"      /* C preprocessor macros */
#endif

/* system includes */
#include <sys/types.h>   /* u_int8_t type */



#ifdef __cplusplus
extern "C" {
#endif

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
int alias (const char *target, const char *dest);


/* resolve_alias_path
   ------------------
   Resolves the Macintosh alias file at the path pointed to
   by path and stores the resolved path in resolved_path.

   The return value is a  pointer to the resolved path upon
   success.  On  error the  return  value is  zero and both
   errno and mac_errno should be consulted.
*/
char* resolve_alias_path (const char *path, char *resolved_path);



/* is_alias
   --------
   Sets the second parameter to true if the file pointed to
   by path is an alias or symbolic link, otherwise sets the
   value to false. Return status the the mac error code.
*/
int is_alias (const char *path, int *is_link);



#ifdef __cplusplus
}
#endif

#endif /* HAVE_APPLE_ALIAS_H */
