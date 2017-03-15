/* This  include file is just to make it easier to include
   the Macintosh CoreServices API.  (by saving keystrokes)

   There's no  reason to  worry about  multiple  inclusion
   problems because the headers included handle it.
*/

/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"   /* C preprocessor macros (ie. HAVE_CORESERVICES) */ 
#endif

/* system includes */
#ifdef HAVE_CORESERVICES
#include <CoreServices/CoreServices.h>
#endif

