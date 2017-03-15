#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "hotline.h"
#include "hxd.h"

int CO = 80;

static int
b_read (int fd, void *bufp, size_t len)
{
	register u_int8_t *buf = (u_int8_t *)bufp;
	register int r, pos = 0;

	while (len) {
		if ((r = read(fd, &(buf[pos]), len)) <= 0)
			return -1;
		pos += r;
		len -= r;
	}

	return pos;
}

static int tracker_break;

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

RETSIGTYPE sigint (void);

RETSIGTYPE
sigint (void)
{
	tracker_break = 1;
}

int
main (int argc, char **argv)
{
	struct sockaddr_in addr;
	u_int16_t port, nusers, nservers;
	unsigned char buf[16], name[512], desc[512];
	struct in_addr a;
	int i, s = 0, mark = 0;
	struct sigaction act, oldact;
	char *co;
	int nlen, dlen;

	if (argc < 2) {
		printf("usage: %s <server> [port]\n", argv[0]);
		return 1;
	}

	co = getenv("COLUMNS");
	if (co)
		CO = strtoul(co, 0, 0);
	memset(&act, 0, sizeof act);
	act.sa_handler = (RETSIGTYPE(*)(int))sigint;
	sigaction(SIGINT, &act, &oldact);

	for (i = 1; i < argc; i++) {
		if (s)
			close(s);

		if (!inet_aton(argv[i], &addr.sin_addr)) {
			struct hostent *he;

			if ((he = gethostbyname(argv[i]))) {
				size_t len = (unsigned)he->h_length > sizeof(struct in_addr)
					     ? sizeof(struct in_addr) : he->h_length;
				memcpy(&addr.sin_addr, he->h_addr, len);
			} else {
#ifndef HAVE_HSTRERROR
				printf("DNS lookup for %s failed\n", argv[i]);
#else
				printf("DNS lookup for %s failed: %s\n", argv[i], hstrerror(h_errno));
#endif
				return 1;
			}
		}

		if (argv[i + 1] && isdigit(argv[i + 1][0]))
			port = atou32(argv[++i]);
		else
			port = HTRK_TCPPORT;

		if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			continue;

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		if (connect(s, (struct sockaddr *)&addr, 16) < 0)
			continue;

		if (write(s, HTRK_MAGIC, HTRK_MAGIC_LEN) != HTRK_MAGIC_LEN)
			continue;

		if (b_read(s, buf, 14) != 14)
			continue;
		printf("%u servers:\n", (nservers = ntohs(*((u_int16_t *)(&(buf[10]))))));
		tracker_break = 0;
		for (; nservers && !tracker_break; nservers--) {
			if (b_read(s, buf, 8) == -1)
				break;
			if (!buf[0]) {	/* wtf ??? */
				nservers++;
				continue;
			}
			mark++;
			if (b_read(s, &buf[8], 3) == -1)
				break;
			a.s_addr = *((u_int32_t *)buf);
			port = ntohs(*((u_int16_t *)(&(buf[4]))));
			nusers = ntohs(*((u_int16_t *)(&(buf[6]))));
			if (b_read(s, name, (size_t)buf[10]) == -1)
				break;
			nlen = buf[10];
			name[(size_t)buf[10]] = 0;
			CR2LF(name, (size_t)buf[10]);
			if (b_read(s, buf, 1) == -1)
				break;
			memset(desc, 0, sizeof desc);
			if (b_read(s, desc, (size_t)buf[0]) == -1)
				break;
			dlen = buf[0];
			desc[(size_t)buf[0]] = 0;
			CR2LF(desc, (size_t)buf[0]);
			printf("%s|%u|%u|%u|%u|\n",
				inet_ntoa(a), port, nusers, nlen, dlen);
			fwrite(name, 1, nlen, stdout);
			printf("\n");
			fwrite(desc, 1, dlen, stdout);
			printf("\n");
		}
	}
	if (s)
		close(s);
	sigaction(SIGINT, &oldact, 0);

	return 0;
}
