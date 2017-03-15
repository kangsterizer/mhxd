#ifndef HAVE_APPLE_FILES_H
#define HAVE_APPLE_FILES_H

/* custom includes */
#if defined(HAVE_CONFIG_H)
#include "config.h"      /* types */
#endif
#include "api.h"         /* mac types */

/* system includes */
#include <sys/types.h>   /* types */



#ifdef __cplusplus
extern "C" {
#endif



/* functions */

int mac_get_type (const char *path, char *type, char *creator);
int mac_set_type (const char *path, const char *type, const char *creator);



#ifdef __cplusplus
}
#endif

#endif /* HAVE_APPLE_FILES_H */
