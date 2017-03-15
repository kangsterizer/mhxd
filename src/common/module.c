#include <stdio.h>
#include <ltdl.h>
#include "hxd.h"
#include "module.h"

void *
module_open (const char *module, int ext)
{
	lt_dlhandle dh;
	char path[4096];

	sprintf(path, "%s/%s", hxd_cfg.paths.modules, module);
	if (ext)
		dh = lt_dlopenext(path);
	else
		dh = lt_dlopen(path);

	return dh;
}

const char *
module_error (void)
{
	return lt_dlerror();
}

void
module_run (void *dh)
{
	lt_ptr sym;
	void (*fn)();

	sym = lt_dlsym(dh, "module_init");
	if (!sym) {
		hxd_log("no module_init in module %p\n", dh);
		return;
	}
	fn = (void (*)())sym;
	fn();
}

void *
module_symbol (void *dh, const char *symbol)
{
	lt_ptr sym;

	sym = lt_dlsym(dh, symbol);

	return sym;
}

void
module_init (void)
{
	lt_dlinit();
}
