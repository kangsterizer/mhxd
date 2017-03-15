#include "hx.h"
#include "hxd.h"
#include <unistd.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <string.h>
#include "xmalloc.h"
#include "conv.h"
#include "translate.h"

#ifdef SOCKS
#include "socks.h"
#endif

#define BABELFISH_IPADDRESS "209.73.180.18"
#define BABELFISH_ENCODING "UTF-8"

#define GET_TEXT ""\
"GET /babelfish/tr?doit=done&tt=urltext&urltext=%s&url=http%%3A%%2F%%2F&lp=%s HTTP/1.0\n"\
"Host: babel.altavista.com:80\n"\
"User-Agent: hx/0.4.9\n\n"

static void
trans_close (struct trans_conn *trans)
{
	int s;

	s = trans->s;
	close(s);
	memset(&hxd_files[s], 0, sizeof(struct hxd_file));
	hxd_fd_clr(s, FDR);
	hxd_fd_clr(s, FDW);
	hxd_fd_del(s);
	xfree(trans);
}

static void
translate_read (int s)
{
	struct trans_conn *trans = (struct trans_conn *)hxd_files[s].conn.ptr;
	struct htlc_conn *htlc;
	struct hx_chat *chat;
	char buf[8192], *p, *stuff;
	size_t stufflen;
	ssize_t r;

	htlc = trans->htlc;
	chat = hx_chat_with_cid(htlc, trans->cid);
	if (!chat) {
		trans_close(trans);
		return;
	}
	r = read(s, buf, sizeof(buf));
/*hx_printf(htlc, 0, "read %d\n",r);
write(2,buf,r);*/
	if (r <= 0) {
		if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EINTR)) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "translate: read: %s\n", strerror(errno));
			trans_close(trans);
		}
		return;
	}
	trans->buf = xrealloc(trans->buf, trans->buflen+r);
	memcpy(trans->buf+trans->buflen, buf, r);
	trans->buflen += r;
	/* p = strstr(trans->buf, "<textarea"); */
	/* if (!p) { */
		p = strstr(trans->buf, "<td bg");
		if (!p)
			return;
		else
			trans->type = 2;
	/* } else
		trans->type = 1; */
	p++;
	p = strchr(p, '>');
	p++;
	p = strchr(p, '>');
	if (!p)
		return;
	p++;
	stuff = p;

	if (trans->type == 1)
		p = strstr(p, "</textarea>");
	else
		p = strstr(p, "</div></td>");
/*hx_printf(htlc, 0, "type=%d p=%p\n",trans->type,p);*/
	if (!p)
		return;
	stufflen = p - stuff;
	*p = 0;
	p--;
	while (*p == '\n' || *p=='\r')
		p--;
	*(p+1) = 0;
	if (!strcmp(trans->fromto, "en_ja")) {
		size_t eucjp_len, sjis_len, utf8_len;
		char *utf8_p, *sjis_p, *eucjp_p;

		utf8_p = stuff;
		utf8_len = stufflen;
		eucjp_len = convbuf(g_encoding, BABELFISH_ENCODING, utf8_p, utf8_len, &eucjp_p);
		if (!eucjp_len) {
			trans_close(trans);
			return;
		}
		hx_printf(htlc, chat, "(%s) %s:  %s\n", trans->fromto, trans->name, eucjp_p);
		xfree(eucjp_p);
		if (TRANS_OUTPUT(trans)) {
			sjis_len = convbuf("SHIFT-JIS", BABELFISH_ENCODING, utf8_p, utf8_len, &sjis_p);
			if (!sjis_len) {
				trans_close(trans);
				return;
			}
			hx_send_chat(htlc, chat->cid, sjis_p, sjis_len, 0);
			xfree(sjis_p);
		}
	} else {
		size_t utf8_len, lat_len;
		char *utf8_p, *lat_p;

		utf8_p = stuff;
		utf8_len = stufflen;
		lat_len = convbuf("ISO-8859-1", BABELFISH_ENCODING, utf8_p, utf8_len, &lat_p);
		if (!lat_len) {
			trans_close(trans);
			return;
		}
		hx_printf(htlc, chat, "(%s) %s:  %s\n", trans->fromto, trans->name, lat_p);
		if (TRANS_OUTPUT(trans))
			hx_send_chat(htlc, chat->cid, lat_p, lat_len, 0);
	}
	trans_close(trans);
}

static void
translate_write (int s)
{
	struct trans_conn *trans = (struct trans_conn *)hxd_files[s].conn.ptr;
	int r;

	if (!TRANS_CONNECTED(trans)) {
		int err = -1;
		socklen_t errlen = sizeof(int);

		getsockopt(s, SOL_SOCKET, SO_ERROR, (SETSOCKOPT_PTR_CAST_T)&err, &errlen);
		if (err != 0) {
			hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "translate: connect so_error %d\n", err);
			trans_close(trans);
			return;
		}
		TRANS_TOGGLE(trans, TRANS_FLAG_CONNECTED);
	}
	fd_blocking(s, 1);
	r = write(s, trans->text, strlen(trans->text));
	fd_blocking(s, 0);
	hxd_fd_clr(s, FDW);
	hxd_fd_set(s, FDR);
}

void
translate_connect (struct trans_conn *trans)
{
	int s, r;
	struct SOCKADDR_IN saddr;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "translate: socket: %s\n", strerror(errno));
		return;
	}
	trans->s = s;
	fd_blocking(s, 0);
 	fd_closeonexec(s, 1);
	saddr.sin_port = htons(80);
	saddr.sin_family = AFINET;
	inet_aton(BABELFISH_IPADDRESS, &saddr.sin_addr);
	r = connect(s, (struct sockaddr *)&saddr, sizeof(saddr));
	if (r < 0) {
		switch (errno) {
			case EINPROGRESS:
				break;
			default:
				hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "translate: connect: %s\n", strerror(errno));
				trans_close(trans);
				return;
				break;
		}
	} else {
		TRANS_TOGGLE(trans, TRANS_FLAG_CONNECTED);
	}
	hxd_files[s].ready_read = translate_read;
	hxd_files[s].ready_write = translate_write;
	hxd_files[s].conn.ptr = (void *)trans;
	hxd_fd_add(s);
	hxd_fd_set(s, FDW);
}

size_t
replacespaces (const char *text, char **newtext)
{
	char *nt, *p, *np;
	size_t tlen;

	tlen = strlen(text);
	nt = xmalloc(3*tlen+1);
	*newtext = nt;
	np = nt;
	for (p = (char *)text; p < text+tlen; p++) {
		if (*p != ' ') {
			*np++ = *p;
			continue;
		}
		*np++ = '%';
		*np++ = '2';
		*np++ = '0';
	}
	*np = 0;

	return tlen;
}

void
translate_chat (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user, const char *text)
{
	struct trans_conn *trans;
	size_t tlen;
	char *newtext;

	trans = xmalloc(sizeof(struct trans_conn));
	memset(trans, 0, sizeof(struct trans_conn));
	trans->htlc = htlc;
	trans->cid = chat->cid;
	strcpy(trans->fromto, user->trans_fromto);
	strcpy(trans->name, user->name);
	trans->uid = user->uid;
	if (TRANS_OUTPUT(user))
		TRANS_TOGGLE(trans, TRANS_FLAG_OUTPUT);
	if (!strcmp(trans->fromto, "ja_en")) {
		size_t sjis_len, utf8_len;
		char *sjis_p, *utf8_p;

		sjis_p = (char *)text;
		sjis_len = strlen(text);
		tlen = strlen(text);
		utf8_len = convbuf(BABELFISH_ENCODING, "SHIFT-JIS", sjis_p, sjis_len, &utf8_p);
		if (!utf8_len) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "error converting SHIFT-JIS to %s\n", BABELFISH_ENCODING);
			xfree(trans);
			return;
		}
		tlen = replacespaces(utf8_p, &newtext);
		xfree(utf8_p);
	} else {
		tlen = strlen(text);
		tlen = replacespaces(text, &newtext);
	}
	trans->text = xmalloc(tlen+256);
	snprintf(trans->text, tlen+256, GET_TEXT, newtext, trans->fromto);
	xfree(newtext);
	translate_connect(trans);
}
