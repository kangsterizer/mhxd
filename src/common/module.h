#ifndef __hxd_module_h
#define __hxd_module_h

extern void *module_open (const char *modulename, int ext);
extern void *module_symbol (void *dh, const char *symbol);
extern void module_init (void);
extern void module_run (void *dh);
extern const char *module_error (void);

#endif
