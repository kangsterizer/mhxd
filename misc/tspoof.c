#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libtreg.h"

static void die (const char *msg) { perror(msg); exit(1); }

void
get_host (struct in_addr *addr, const char *host)
{
	if (!inet_aton(host, addr)) {
		struct hostent *he = gethostbyname(host);
		if (he)
			memcpy(addr, he->h_addr_list[0],
				(u_int)he->h_length > sizeof(struct in_addr)
				? sizeof(struct in_addr)
				: (u_int)he->h_length);
		else
			die(host);
	}
}

int
main (int argc, char **argv)
{
	char *dst_host, *src_host = 0, *fake_host = 0;
	struct in_addr dst_addr, src_addr, fake_addr;
	char *name = "0wn3d", *desc = "0wn3d server";
	u_int nusers = 31337;
	u_int nservers, port, nlen, dlen;
	char nbuf[256], dbuf[256], shbuf[256];
	u_int i;
	char buf[1024];

	nlen = strlen(name);
	dlen = strlen(desc);
#ifdef TSPOOF
	src_host = "127.0.0.1";
#endif
	dst_host = "127.0.0.1";

	if (!isatty(0)) {
		if (argc > 1)
			dst_host = argv[1];
		if (argc > 2)
			fake_host = argv[2];
		get_host(&dst_addr, dst_host);
		if (fake_host)
			get_host(&fake_addr, fake_host);
		fscanf(stdin, "%u servers:\n", &nservers);
		for (i = 0; i < nservers; i++) {
			int r;
			fgets(buf, sizeof(buf)-1, stdin);
			r = sscanf(buf, "%[^|]|%u|%u|%u|%u|\n",
					shbuf, &port, &nusers, &nlen, &dlen);
			if (r != 5)
				break;
			fread(nbuf, 1, nlen, stdin);
			fread(buf, 1, 1, stdin);
			nbuf[nlen]=0;
			fread(dbuf, 1, dlen, stdin);
			fread(buf, 1, 1, stdin);
			dbuf[dlen]=0;

			get_host(&src_addr, shbuf);
			if (fake_host)
				treg_add(&dst_addr, &src_addr, &fake_addr, port, 0, nbuf, dbuf, nusers);
			else
				treg_add(&dst_addr, &src_addr, &src_addr, port, 0, nbuf, dbuf, nusers);
		}
	} else {
		/* args: tracker_host src_host name desc */
		if (argc > 1) {
			dst_host = argv[1];
			if (argc > 2)
				src_host = argv[2];
			if (argc > 3) {
				name = argv[3];
				nlen = strlen(name);
			}
			if (argc > 4) {
				desc = argv[4];
				dlen = strlen(desc);
			}
		} else {
			dst_host = "127.0.0.1";
		}
		get_host(&dst_addr, dst_host);
		if (src_host)
			get_host(&src_addr, src_host);
		else
			memset(&src_addr, 0, sizeof(src_addr));
		treg_add(&dst_addr, &src_addr, &src_addr, 5500, 0, name, desc, nusers);
	}

	treg_loop();

	return 0;
}
