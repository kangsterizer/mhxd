#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#endif
#include <fnmatch.h>
#include "hlserver.h"
#include "xmalloc.h"
#include "getline.h"

#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif

#define IDENT_TCPPORT	113

struct banent { 
	struct banent *next;
	time_t expires;
	u_int16_t vmode;
	u_int16_t version;
	char name[32];
	char login[32];
	char userid[32];
	char address[16];
	char message[4];
};

#define VMODE_GREATER	1
#define VMODE_LESS	2
#define VMODE_EQUAL	4

static struct banent *ban_list = 0;

void
read_banlist (void)
{
	char *p;
	char expires[128], vstr[16], name[32], login[32], userid[32], address[16], message[256];
	int fd, r;
	struct banent *be, *nextbe, dummy_be;
	getline_t *gl;

	for (be = ban_list; be; be = nextbe) {
		nextbe = be->next;
		xfree(be);
	}
	dummy_be.next = 0;
	be = &dummy_be;
	fd = SYS_open(hxd_cfg.paths.banlist, O_RDONLY, 0);
	if (fd < 0)
		return;
	gl = getline_open(fd);
	for (;;) {
		p = getline_line(gl, &r);
		if (!p)
			break;
		while (isspace(*p))
			p++;
		if (*p == '#')
			continue;
		message[0] = 0;
		if (sscanf(p, "%127s%15s%31s%31s%31s%15s%*[\t ]%255[^\n]",
			   expires, vstr, name, login, userid, address, message) < 7)
			continue;
		be->next = xmalloc(sizeof(struct banent) + strlen(message));
		be = be->next;
		be->next = 0;
		be->vmode = 0;
		switch (vstr[0]) {
			case '>':
			case '<':
				be->vmode = vstr[0] == '>' ? VMODE_GREATER : VMODE_LESS;
				if (vstr[1] == '=') {
					be->vmode |= VMODE_EQUAL;
					be->version = atou16(&vstr[2]);
				} else {
					be->version = atou16(&vstr[1]);
				}
				break;
			case '*':
				be->version = 0;
				be->vmode = VMODE_GREATER | VMODE_EQUAL;
				break;
			default:
				be->version = atou16(vstr);
				break;
		}
		strcpy(be->name, name);
		strcpy(be->login, login);
		strcpy(be->userid, userid);
		strcpy(be->address, address);
		strcpy(be->message, message);
		if (!strcmp(expires, "never")) {
			be->expires = (time_t)-1;
		} else {
			struct tm tm;
			memset(&tm, 0, sizeof(tm));
			if (strptime(expires, "%Y:%m:%d:%H:%M:%S", &tm))
				be->expires = mktime(&tm);
			else
				be->expires = (time_t)-1;
		}
	}
	close(fd);
	getline_close(gl);
	ban_list = dummy_be.next;
}

int
check_banlist (struct htlc_conn *htlc)
{
	struct banent *be, *prevbe, *nextbe, dummy_be;
	time_t now;
	char msg[512];
	char abuf[HOSTLEN+1];
	int fd, len;
	int versionbanned;

	inaddr2str(abuf, &htlc->sockaddr);
	now = time(0);
	dummy_be.next = ban_list;
	prevbe = &dummy_be;
	for (be = ban_list; be; be = nextbe) {
		nextbe = be->next;
		if (be->expires != (time_t)-1 && be->expires <= now) {
			xfree(be);
			if (prevbe)
				prevbe->next = nextbe;
			if (be == ban_list)
				ban_list = nextbe;
			continue;
		}
		versionbanned = 0;
		switch (be->vmode&3) {
			case VMODE_GREATER:
				if (be->vmode & VMODE_EQUAL)
					versionbanned = htlc->clientversion >= be->version;
				else
					versionbanned = htlc->clientversion > be->version;
				break;
			case VMODE_LESS:
				if (be->vmode & VMODE_EQUAL)
					versionbanned = htlc->clientversion <= be->version;
				else
					versionbanned = htlc->clientversion < be->version;
				break;
		}
		if ((!htlc->name[0] ? be->name[0] == '*' && !be->name[1] : !fnmatch(be->name, htlc->name, FNM_NOESCAPE))
		    && (!htlc->login[0] ? be->login[0] == '*' && !be->login[1] : !fnmatch(be->login, htlc->login, FNM_NOESCAPE))
		    && (!htlc->userid[0] ? be->userid[0] == '*' && !be->userid[1] : !fnmatch(be->userid, htlc->userid, FNM_NOESCAPE))
		    && !fnmatch(be->address, abuf, FNM_NOESCAPE) && versionbanned) {
			char vstr[16];
			snprintf(vstr, sizeof(vstr), "%c%s%u", (be->vmode&3)==VMODE_GREATER?'>':'<', be->vmode&VMODE_EQUAL?"=":"", be->version);
			hxd_log("banned: %s:%s!%s@%s (%s:%s:%s!%s@%s)", htlc->name, htlc->login, htlc->userid, abuf,
									vstr, be->name, be->login, be->userid, be->address);
			len = sprintf(msg, "you are banned: %s", be->message);
			snd_errorstr(htlc, msg);
			htlc_flush_close(htlc);
			return 1;
		}
		prevbe = be;
	}

	return 0;
}

void
addto_banlist (time_t t, const char *name, const char *login, const char *userid, const char *address, const char *message)
{
	struct banent *be;

	be = xmalloc(sizeof(struct banent) + strlen(message));
	strcpy(be->name, name);
	strcpy(be->login, login);
	strcpy(be->userid, userid);
	strcpy(be->address, address);
	strcpy(be->message, message);
	be->version = 0;
	be->vmode = VMODE_GREATER | VMODE_EQUAL;
	if (!ban_list) {
		ban_list = be;
		be->next = 0;
	} else {
		be->next = ban_list->next;
		ban_list->next = be;
	}
	be->expires = time(0) + t;
}

void
ident_close (int fd)
{
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;

	socket_close(fd);
	nr_open_files--;
	hxd_fd_clr(fd, FDR|FDW);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	if (high_fd == fd) {
		for (fd--; fd && !FD_ISSET(fd, &hxd_rfds); fd--)
			;
		high_fd = fd;
	}
	htlc->identfd = -1;
	hxd_fd_set(htlc->fd, FDR);
	if (high_fd < htlc->fd)
		high_fd = htlc->fd;
	/* qbuf_set(&htlc->in, 0, HTLC_MAGIC_LEN); */
	qbuf_set(&htlc->in, 0, 0);
}

static void
ident_write (int fd)
{
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
	struct SOCKADDR_IN saddr;
	int x;
	char buf[32];
	int len;

	hxd_fd_clr(fd, FDW);
	hxd_fd_set(fd, FDR);
	x = sizeof(saddr);
	if (getsockname(htlc->fd, (struct sockaddr *)&saddr, &x)) {
		hxd_log("%s:%d: getsockname: %s", __FILE__, __LINE__, strerror(errno));
		goto bye;
	}

	len = snprintf(buf, sizeof(buf), "%u , %u\r\n", ntohs(htlc->sockaddr.SIN_PORT), ntohs(saddr.SIN_PORT));

	if (write(fd, buf, len) != len) {
bye:
		ident_close(fd);
	}
}

static void
ident_read (int fd)
{
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
	char *s, *t;
	char ruser[512+1], system[64+1];
	u_int16_t remp = 0, locp = 0;
	int r;
#if defined(__WIN32__)
	int wsaerr;
#endif

	r = socket_read(fd, &htlc->in.buf[htlc->in.pos], htlc->in.len);
	if (r <= 0) {
#if defined(__WIN32__)
		wsaerr = WSAGetLastError();
		if (r == 0 || (r < 0 && wsaerr != WSAEWOULDBLOCK && wsaerr != WSAEINTR))
#else
		if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EINTR))
#endif
			goto bye;
		return;
	}
	htlc->in.pos += r;
	htlc->in.len -= r;
	if (htlc->in.len <= 1)
		goto bye;
	htlc->in.buf[htlc->in.pos] = 0;

	if (sscanf(htlc->in.buf, "%hd , %hd : USERID : %*[^:]: %512s", &remp, &locp, ruser) == 3) {
		s = strrchr(htlc->in.buf, ':');
		*s++ = '\0';
		for (t = (strrchr(htlc->in.buf, ':') + 1); *t; t++)
			if (!isspace(*t))
				break;
		strlcpy(system, t, sizeof(system));
		for (t = ruser; *s && t < ruser + 512; s++)
			if (!isspace(*s) && *s != ':' && *s != '@')
				*t++ = *s;
		*t = '\0';
		strcpy(htlc->userid, ruser);
		if (!ruser)
			sprintf(htlc->userid, "~");
	} else {
		if (!strstr(htlc->in.buf, "\r\n"))
			return;
	}
bye:
	ident_close(fd);
	check_banlist(htlc);
}

void
start_ident (struct htlc_conn *htlc)
{
	struct SOCKADDR_IN saddr;
	int x;
	int s;

	s = socket(AFINET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		hxd_log("%s:%d: socket: %s", __FILE__, __LINE__, strerror(errno));
		goto ret;
	}
	nr_open_files++;
	if (nr_open_files >= hxd_open_max) {
		hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, s, hxd_open_max);
		goto close_and_ret;
	}
	fd_closeonexec(s, 1);
	socket_blocking(s, 0);

	x = sizeof(saddr);
	if (getsockname(htlc->fd, (struct sockaddr *)&saddr, &x)) {
		hxd_log("%s:%d: getsockname: %s", __FILE__, __LINE__, strerror(errno));
		goto close_and_ret;
	}
	saddr.SIN_PORT = 0;
	saddr.SIN_FAMILY = AFINET;
	if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr))) {
		hxd_log("%s:%d: bind: %s", __FILE__, __LINE__, strerror(errno));
		goto close_and_ret;
	}

	saddr.SIN_ADDR = htlc->sockaddr.SIN_ADDR;
	saddr.SIN_PORT = htons(IDENT_TCPPORT);
	saddr.SIN_FAMILY = AFINET;
#if defined(__WIN32__)
	if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) && WSAGetLastError() != WSAEINPROGRESS) {
#else
	if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) && errno != EINPROGRESS) {
#endif
		/* hxd_log("%s:%d: connect: %s", __FILE__, __LINE__, strerror(errno)); */
		goto close_and_ret;
	}

	hxd_files[s].conn.htlc = htlc;
	hxd_files[s].ready_read = ident_read;
	hxd_files[s].ready_write = ident_write;
	hxd_fd_set(s, FDW);
	if (high_fd < s)
		high_fd = s;
	htlc->identfd = s;
	qbuf_set(&htlc->in, 0, 1024);
	return;

close_and_ret:
	socket_close(s);
	nr_open_files--;
ret:
	htlc->identfd = -1;
	/* qbuf_set(&htlc->in, 0, HTLC_MAGIC_LEN); */
	qbuf_set(&htlc->in, 0, 0); 
	hxd_fd_set(htlc->fd, FDR);
	if (high_fd < htlc->fd)
		high_fd = htlc->fd;
}
