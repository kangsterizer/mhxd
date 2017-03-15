#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include "sys_net.h"
#include "hotline.h"
#include "hldump.h"

#if defined(CONFIG_HLDUMP)

#if defined(ONLY_SERVER)
#include "hlserver.h"
#include <string.h>
#include <errno.h>

static FILE *hldump_fp = 0;

void
hldump_init (const char *path)
{
	if (hldump_fp) {
		fclose(hldump_fp);
	}
	if (!hxd_cfg.options.hldump) {
		hldump_fp = 0;
		return;
	}
	hldump_fp = fopen(path, "a");
	if (!hldump_fp) {
		hxd_log("fopen(%s): %s", hxd_cfg.paths.hldump, strerror(errno));
		return;
	}
}
#endif

static void
pr (const char *fmt, ...)
{
	FILE *fp;
	va_list ap;

#if defined(ONLY_SERVER)
	fp = hldump_fp;
#else
	fp = stderr;
#endif
	if (!fp)
		return;
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fflush(fp);
}

static void
hexdump (const unsigned char *s, size_t len)
{
	register unsigned char const *p, *end;

	end = s+len;
	for (p = s; p < end; p++)
		pr("%2.2x", (unsigned int)(*p));
}

static void
pr_h (struct hl_hdr *h)
{
	char *typestr;
	u_int32_t type, i;
	struct hl_hdr_type *te;

	type = ntohl(h->type);
	typestr = 0;
	for (i = 0; i < sizeof(hl_h_types)/sizeof(struct hl_hdr_type); i++) {
		te = &hl_h_types[i];
		if (te->type == type) {
			typestr = te->name;
			break;
		}
	}
	if (!typestr)
		typestr = "UNKNOWN";
	pr("type  = %08x (%s)\n", type, typestr);
	pr("trans = %08x\n", ntohl(h->trans));
	pr("flag  = %08x\n", ntohl(h->flag));
	pr("totlen= %08x\n", ntohl(h->totlen));
	pr("len   = %08x\n", ntohl(h->len));
	pr("hc    = %04x\n", ntohs(h->hc));
}

static void
pr_dh (struct hl_data_hdr *dh)
{
	char *typestr;
	u_int16_t type, i;
	struct hl_data_hdr_type *te;

	type = ntohs(dh->type);
	typestr = 0;
	for (i = 0; i < sizeof(hl_dh_types)/sizeof(struct hl_data_hdr_type); i++) {
		te = &hl_dh_types[i];
		if (te->type == type) {
			typestr = te->name;
			break;
		}
	}
	if (!typestr)
		typestr = "UNKNOWN";
	pr("dh_type = %04x (%s)\n", type, typestr);
	pr("dh_len  = %04x\n", ntohs(dh->len));
	pr("dh_data:\n");
	hexdump(dh->data, ntohs(dh->len));
	pr("\n");
}

void
hldump_packet (void *__pktbuf, u_int32_t pktbuflen)
{
	u_int8_t *pktbuf;
	struct hl_hdr *h;
	struct hl_data_hdr *dh;
	u_int32_t pos;
	u_int16_t hc, i;

	pktbuf = (u_int8_t *)__pktbuf;
	if (pktbuflen < SIZEOF_HL_HDR)
		return;
	h = (struct hl_hdr *)pktbuf;
	pr_h(h);

	hc = ntohs(h->hc);
	pos = SIZEOF_HL_HDR;
	for (i = 0; i < hc; i++) {
		dh = (struct hl_data_hdr *)(pktbuf + pos);
		pos += SIZEOF_HL_DATA_HDR + ntohs(dh->len);
		if (pos > pktbuflen)
			break;
		pr_dh(dh);
	}
}
#endif /* CONFIG_HLDUMP */
