#ifndef _XMALLOC_H
#define _XMALLOC_H

#include <sys/types.h>

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

extern void *xmalloc (size_t size);
extern void *xrealloc (void *ptr, size_t size);
extern void xfree (void *ptr);
extern char *xstrdup (const char *str);

#if XMALLOC_DEBUG
extern void DTBLINIT (void);
#endif

#endif /* ndef _XMALLOC_H */
