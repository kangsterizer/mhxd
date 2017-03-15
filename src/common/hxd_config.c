#include <string.h>
#include "conf.h"
#include "hxd.h"
#include "hxd_config.h"

#include "hxd_wanted.c"

struct hxd_config hxd_cfg;

void
hxd_config_init (struct hxd_config *cfg)
{
	static int got_default = 0;

	memset(cfg, 0, sizeof(struct hxd_config));
	if (!got_default) {
#if defined(CONFIG_CONFSOFF_DYNAMIC)
		hxd_wanted_init();
#endif
		conf_wanted_to_config(&hxd_config_default, &hxd_wanted_t0s0);
		got_default = 1;
	}
	memcpy(cfg, &hxd_config_default, sizeof(struct hxd_config));
}

void
hxd_config_read (const char *fname, void *cfgmem, int perr)
{
	struct hxd_config *cfg = (struct hxd_config *)cfgmem;
	struct wanted *wtop;

	wtop = conf_read(fname, &hxd_wanted_t0s0);
	if (!wtop) {
		if (perr)
			hxd_log("error reading %s", fname);
		return;
	}
	conf_wanted_to_config(cfg, wtop);
	conf_wanted_freevars(wtop);
}

void
hxd_config_free (void *cfgmem)
{
	conf_config_free(cfgmem, &hxd_wanted_t0s0);
}
