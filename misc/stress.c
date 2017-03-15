#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hxd.h"

void
hlwrite (struct htlc_conn *htlc, u_int32_t type, u_int32_t flag, int hc, ...)
{
	va_list ap;
	struct hl_hdr h;
	struct hl_data_hdr dhs;
	struct qbuf __qbuf, *q;
	u_int32_t this_off, pos, len;
	int s = htlc->fd;

	if (!htlc->fd)
		return;

	memset(&__qbuf, 0, sizeof(__qbuf));
	q = &__qbuf;
	this_off = q->pos + q->len;
	pos = this_off + SIZEOF_HL_HDR;
	q->len += SIZEOF_HL_HDR;
	q->buf = realloc(q->buf, q->pos + q->len);

	h.type = htonl(type);
	h.trans = htonl(htlc->trans);
	htlc->trans++;
	h.flag = htonl(flag);
	h.hc = htons(hc);

	va_start(ap, hc);
	while (hc) {
		u_int16_t t, l;
		u_int8_t *data;

		t = (u_int16_t)va_arg(ap, int);
		l = (u_int16_t)va_arg(ap, int);
		dhs.type = htons(t);
		dhs.len = htons(l);

		q->len += SIZEOF_HL_DATA_HDR + l;
		q->buf = realloc(q->buf, q->pos + q->len);
		memory_copy(&q->buf[pos], (u_int8_t *)&dhs, 4);
		pos += 4;
		data = va_arg(ap, u_int8_t *);
		if (l) {
			memory_copy(&q->buf[pos], data, l);
			pos += l;
		}
		hc--;
	}
	va_end(ap);

	len = pos - this_off;
	h.len = htonl(len - (SIZEOF_HL_HDR - sizeof(h.hc)));
	h.len2 = h.len;

	memory_copy(q->buf + this_off, &h, SIZEOF_HL_HDR);
	write(s, q->buf, len);
	free(q->buf);
}

void
hl_code (void *__dst, const void *__src, size_t len)
{
	u_int8_t *dst = (u_int8_t *)__dst, *src = (u_int8_t *)__src;

	for (; len; len--)
		*dst++ = ~*src++;
}

int
fd_blocking (int fd, int on)
{
	int x;

#if defined(_POSIX_SOURCE) || !defined(FIONBIO)
#if !defined(O_NONBLOCK)
# if defined(O_NDELAY)
#  define O_NONBLOCK O_NDELAY
# endif
#endif
	if ((x = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
	if (on)
		x &= ~O_NONBLOCK;
	else
		x |= O_NONBLOCK;

	return fcntl(fd, F_SETFL, x);
#else
	x = !on;

	return ioctl(fd, FIONBIO, &x);
#endif
}

struct htlc_conn hx_htlc;

int
main (int argc, char **argv)
{
	struct sockaddr_in saddr;
	fd_set writefds, wfds;
	int n, s, high_s;
	unsigned int i;
	u_int16_t ic;

	memset(&hx_htlc, 0, sizeof(hx_htlc));
	inet_aton(argv[1] ? argv[1] : "127.0.0.1", &saddr.sin_addr);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(5500);
	for (i = 0; i < 250; i++) {
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0) {
			perror("socket");
			exit(1);
		}
		fd_blocking(s, 0);
		if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr))) {
			if (errno != EINPROGRESS) {
				perror("connect");
				exit(1);
			}
		}
		FD_SET(s, &writefds);
	}
	high_s = s;
	for (;;) {
		wfds = writefds;
		n = select(high_s + 1, 0, &wfds, 0, 0);
		if (n < 0) {
			perror("select");
			exit(1);
		}
		for (s = 3; n && s <= high_s; s++) {
			if (FD_ISSET(s, &wfds)) {
				int siz = sizeof(saddr);
				if (!getpeername(s, (struct sockaddr *)&saddr, &siz)) {
					unsigned char magic[8];
					fd_blocking(s, 1);
					write(s, "TRTPHOTL\0\1\0\2", 12);
					if (read(s, magic, 8) == 8 && !memcmp(magic, "TRTP\0\0\0\0", 8)) {
						printf("connected %d\n", s);
						hx_htlc.fd = s;
		ic = htons(3586);
		hlwrite(&hx_htlc, HTLC_HDR_LOGIN, 0, 2,
		//	HTLC_DATA_LOGIN, strlen(login), login,
			HTLC_DATA_NAME, strlen("    who loves heidi?"), "    who loves heidi?",
		//	HTLC_DATA_PASSWORD, strlen(password), password,
			HTLC_DATA_ICON, 2, &ic);
					}
				}
				FD_CLR(s, &writefds);
				n--;
			}
		}
	}

	return 0;
}
