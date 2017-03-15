#include "hx.h"
#include "hxd.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef SOCKS
#include "socks.h"
#endif

static int
b_read (int fd, void *bufp, size_t len)
{
	register u_int8_t *buf = (u_int8_t *)bufp;
	register int r, pos = 0;

	while (len) {
		if ((r = socket_read(fd, &(buf[pos]), len)) <= 0)
			return -1;
		pos += r;
		len -= r;
	}

	return pos;
}

static int tracker_break;

static RETSIGTYPE
tracker_sigint (int sig __attribute__((__unused__)))
{
	tracker_break = 1;
}

static void
tracker_close (int s)
{
	hxd_fd_clr(s, FDR|FDW);
	hxd_fd_del(s);
	memset(&hxd_files[s], 0, sizeof(struct hxd_file));
	close(s);
}

static void
tracker_ready_write (int s)
{
	hxd_fd_clr(s, FDW);
	if (write(s, HTRK_MAGIC, HTRK_MAGIC_LEN) != HTRK_MAGIC_LEN) {
		tracker_close(s);
		return;
	}
	hxd_fd_set(s, FDR);
}

static void
tracker_ready_read (int s)
{
	u_int16_t port, nusers, nservers;
#ifdef CONFIG_IPV6
	unsigned char buf[HOSTLEN+1];
#else
	unsigned char buf[16];
#endif
	unsigned char name[512], desc[512];
	struct IN_ADDR a;
#if !defined(__WIN32__)
	struct sigaction act, oldact;

	memset(&act, 0, sizeof(act));
	act.sa_handler = tracker_sigint;
	sigaction(SIGINT, &act, &oldact);
#endif

	if (b_read(s, buf, 14) != 14)
		goto funk_dat;
	nservers = ntohs(*((u_int16_t *)(&(buf[10]))));
	hx_printf_prefix(hxd_files[s].conn.htlc, 0, INFOPREFIX, "%u servers:\n", nservers);
	for (; nservers && !tracker_break; nservers--) {
		if (b_read(s, buf, 8) == -1)
			break;
		if (!buf[0]) {	/* assuming an address does not begin with 0, we can skip this */
			nservers++;
			continue;
		}
		if (b_read(s, buf+8, 3) == -1)
			break;
		/* XXX: Tracker uses 4 bytes for IPv4 addresses */
		/* It must use 16 bites for IPv6 addresses */
#ifdef CONFIG_IPV6
#warning IPv6 Tracker is b0rked
		/*	a.S_ADDR = *((u_int32_t *)buf); */
#else
		a.S_ADDR = *((u_int32_t *)buf);
#endif
		port = ntohs(*((u_int16_t *)(&(buf[4]))));
		nusers = ntohs(*((u_int16_t *)(&(buf[6]))));
		if (b_read(s, name, (size_t)buf[10]) == -1)
			break;
		name[(size_t)buf[10]] = 0;
		CR2LF(name, (size_t)buf[10]);
		strip_ansi(name, (size_t)buf[10]);
		if (b_read(s, buf, 1) == -1)
			break;
		memset(desc, 0, sizeof(desc));
		if (b_read(s, desc, (size_t)buf[0]) == -1)
			break;
		desc[(size_t)buf[0]] = 0;
		CR2LF(desc, (size_t)buf[0]);
		strip_ansi(desc, (size_t)buf[0]);
#ifdef CONFIG_IPV6
		inet_ntop(AFINET, (char *)&a, buf, sizeof(buf));
#else
		inet_ntoa_r(a, buf, sizeof(buf));
#endif
		hx_output.tracker_server_create(hxd_files[s].conn.htlc, buf, port, nusers, name, desc);
	}
funk_dat:
	hxd_fd_clr(s, FDR);
	socket_close(s);
#if !defined(__WIN32__)
	sigaction(SIGINT, &oldact, 0);
#endif
}

void
hx_tracker_list (struct htlc_conn *htlc, struct hx_chat *chat, const char *serverstr, u_int16_t port)
{
	struct SOCKADDR_IN saddr;
	int s;

	tracker_break = 0;
#ifdef CONFIG_IPV6
	if (!inet_pton(AFINET, serverstr, &saddr.SIN_ADDR)) {
#else
	if (!inet_aton(serverstr, &saddr.SIN_ADDR)) {
#endif
		struct hostent *he;

		if ((he = gethostbyname(serverstr))) {
			size_t len = (unsigned)he->h_length > sizeof(struct IN_ADDR)
				     ? sizeof(struct IN_ADDR) : (unsigned)he->h_length;
			memcpy(&saddr.SIN_ADDR, he->h_addr, len);
		} else {
#ifndef HAVE_HSTRERROR
			hx_printf_prefix(htlc, chat, INFOPREFIX, "DNS lookup for %s failed\n", serverstr);
#else
			hx_printf_prefix(htlc, chat, INFOPREFIX, "DNS lookup for %s failed: %s\n", serverstr, hstrerror(h_errno));
#endif
			return;
		}
	}

	if ((s = socket(AFINET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "tracker: %s: %s", serverstr, strerror(errno));
		return;
	}

	saddr.SIN_FAMILY = AFINET;
	saddr.SIN_PORT = htons(port);

	socket_blocking(s, 0);
#if defined(__WIN32__)
#define EINPROGRESS -1
#endif
	if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		if (errno != EINPROGRESS) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "tracker: %s: %s", serverstr, strerror(errno));
			close(s);
			return;
		}
	}
	socket_blocking(s, 1);
	hxd_files[s].conn.htlc = htlc;
	hxd_files[s].ready_read = tracker_ready_read;
	hxd_files[s].ready_write = tracker_ready_write;
	hxd_fd_add(s);
	hxd_fd_set(s, FDW);
}
