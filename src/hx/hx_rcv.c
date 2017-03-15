#include "hx.h"
#include "hxd.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "sys_net.h"
#include "xmalloc.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#if defined(CONFIG_SOUND)
#include "sound.h"
#endif

#if defined(CONFIG_HAL)
extern int hal_active;
extern void hal_rcv (struct htlc_conn *htlc, char *input, char *nick);
#endif

extern struct hx_chat *tty_chat_front;

extern void away_log (const char *fmt, ...);

#if defined(CONFIG_TRANSLATE)
extern void translate_chat (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user, const char *chat_part);

static char *chat_seps[2] = {
	" - ",
	":  "
};
#define NSEPS 2

static size_t
name_from_chat (char *chat, char *name, char **chat_part)
{
	char *p;
	unsigned int i, found;
	size_t nlen;

	found = 0;
	for (i = 0; i < NSEPS; i++) {
		p = strstr(chat, chat_seps[i]);
		if (p) {
			found = 1;
			break;
		}
	}
	if (!found)
		return 0;
	if (chat_part)
		*chat_part = p+3;
	nlen = p - chat;
	if (nlen > 31)
		return 0;
	p = chat;
	while (*p == ' ' && *p)
		p++;
	nlen -= (p - chat);
	memcpy(name, p, nlen);
	name[nlen] = 0;

	return nlen;
}

static char *
chat_from_chat (char *chat)
{
	char *p;
	unsigned int i;

	for (i = 0; i < NSEPS; i++) {
		p = strstr(chat, chat_seps[i]);
		if (p)
			return (p+3);
	}

	return chat;

}
#endif

static void
hx_rcv_chat (struct htlc_conn *htlc)
{
	u_int32_t cid = 0;
	u_int16_t len = 0;
	u_int32_t uid = 0;
	char chatbuf[8192 + 4], *chat;
#if defined(CONFIG_ICONV)
	char *out_p = 0;
	size_t out_len;
#endif

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_CHAT:
				len = (dh_len > (sizeof(chatbuf)-4) ? (sizeof(chatbuf)-4) : dh_len);
				memcpy(chatbuf, dh_data, len);
				break;
			case HTLS_DATA_CHAT_ID:
				dh_getint(cid);
				break;
			case HTLS_DATA_UID:
				dh_getint(uid);
				break;
		}
	dh_end()
	if (uid) {
		struct hx_chat *hxchat;
		struct hx_user *user;

		hxchat = hx_chat_with_cid(htlc, cid);
		if (hxchat) {
			user = hx_user_with_uid(hxchat->user_list, uid);
			if (user && USER_IGNORE(user))
				return;
		}
	}
	/* Try to get sound started early. */
	play_sound(snd_chat);

	CR2LF(chatbuf, len);
	strip_ansi(chatbuf, len);
	if (chatbuf[0] == '\n') {
		chat = chatbuf+1;
		if (len)
			len--;
	} else {
		chat = chatbuf;
	}
#if defined(CONFIG_ICONV)
	out_len = convbuf(g_encoding, g_server_encoding, chat, len, &out_p);
	if (out_len) {
		if (len < out_len)
			len = out_len;
		chat = out_p;
	}
#endif
	chat[len] = 0;
	hx_output.chat(htlc, cid, chat, len);
#if defined(CONFIG_HAL)
	if (hal_active) {
		char nickname[32], *user, *sep, *lf, ss;

		chat[len] = 0;
		sep = chat + 13;
		if ((sep[0] == ':' && sep[1] == ' ' && sep[2] == ' ')
		    || (sep[0] == ' ' && sep[1] == '-' && sep[2] == ' ')) {
			ss = sep[0];
			sep[0] = 0;
			user = chat;
			while (*user == ' ' && *user)
				user++;
			strcpy(nickname, user);
			sep[0] = ss;
			if (strcmp(nickname, htlc->name)) {
				sep += 3;
				lf = strchr(sep, '\n');
				if (lf)
					*lf = 0;
				hal_rcv(htlc, sep, nickname);
				if (lf)
					*lf = '\n';
			}
		}
	}
#endif
#if defined(CONFIG_TRANSLATE)
{
	struct hx_chat *hxchat;
	struct hx_user *user;
	char *chat_part;

	hxchat = hx_chat_with_cid(htlc, cid);
	if (!hxchat)
		goto end_trans;
	if (uid) {
		user = hx_user_with_uid(hxchat->user_list, uid);
		if (!user || !USER_TRANSLATE(user))
			goto end_trans;
		chat_part = chat_from_chat(chat);
	} else {
		char name[32];
		size_t nlen;

		nlen = name_from_chat(chat, name, &chat_part);
		if (nlen) {
			user = hx_user_with_name(hxchat->user_list, name);
			if (!user || !USER_TRANSLATE(user))
				goto end_trans;
		} else {
			goto end_trans;
		}
	}
	if (!USER_TRANSLATE(user) || !strcmp(user->name, htlc->name))
		goto end_trans;

	chat[len] = 0;
	translate_chat(htlc, hxchat, user, chat_part);
}
end_trans:
#endif
#if defined(CONFIG_ICONV)
	if (out_p)
		xfree(out_p);
#endif
}

static void
hx_rcv_msg (struct htlc_conn *htlc)
{
	u_int32_t uid = 0;
	u_int16_t msglen = 0, nlen = 0;
	char *msg = "", *name = "";
	struct hx_user *user = 0;
	struct hx_chat *chat = 0;
#if defined(CONFIG_ICONV)
	char *out_p = 0;
	size_t out_len;
#endif

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_UID:
				dh_getint(uid);
				break;
			case HTLS_DATA_MSG:
				msglen = dh_len;
				msg = dh_data;
				break;
			case HTLS_DATA_NAME:
				nlen = dh_len;
				name = dh_data;
				break;
		}
	dh_end()
	chat = hx_chat_with_cid(htlc, 0);
	if (chat) {
		user = hx_user_with_uid(chat->user_list, uid);
		if (user && USER_IGNORE(user)) {
			return;
		}
	}

	if (msglen) {
#if defined(CONFIG_ICONV)
		out_len = convbuf(g_encoding, g_server_encoding, msg, msglen, &out_p);
		if (out_len) {
			if (msglen < out_len)
				msglen = out_len;
			msg = out_p;
		}
#endif
		CR2LF(msg, msglen);
		strip_ansi(msg, msglen);
		msg[msglen] = 0;
	}
	if (nlen)
		name[nlen] = 0;
	away_log("[%s(%u)]: %s\n", name, uid, msg);
	hx_output.msg(htlc, uid, name, msg, msglen);
	play_sound(snd_message);
	if (!*last_msg_nick)
		strlcpy(last_msg_nick, name, 32);
#if defined(CONFIG_ICONV)
	if (out_p)
		xfree(out_p);
#endif
}

static void
hx_rcv_agreement_file (struct htlc_conn *htlc)
{
	dh_start(htlc)
		if (dh_type != HTLS_DATA_AGREEMENT)
			continue;
		CR2LF(dh_data, dh_len);
		strip_ansi(dh_data, dh_len);
		hx_output.agreement(htlc, dh_data, dh_len);
	dh_end()
	if (htlc->clientversion >= 150 && htlc->serverversion >= 150) {
		u_int16_t icon16;

		icon16 = htons(htlc->icon);
		htlc->trans = 2;
		task_new(htlc, 0, 0, 0, "agree");
		hlwrite(htlc, HTLC_HDR_AGREEMENTAGREE, 0, 3,
			HTLC_DATA_NAME, strlen(htlc->name), htlc->name,
			HTLC_DATA_ICON, 2, &icon16,
			HTLC_DATA_BAN, 2, "\0\0");
	}
}

static void
hx_rcv_news_post (struct htlc_conn *htlc)
{
	dh_start(htlc)
		if (dh_type != HTLS_DATA_NEWS)
			continue;
		htlc->news_len += dh_len;
		htlc->news_buf = xrealloc(htlc->news_buf, htlc->news_len + 1);
		memmove(&(htlc->news_buf[dh_len]), htlc->news_buf, htlc->news_len - dh_len);
		memcpy(htlc->news_buf, dh_data, dh_len);
		CR2LF(htlc->news_buf, dh_len);
		strip_ansi(htlc->news_buf, dh_len);
		htlc->news_buf[htlc->news_len] = 0;
		hx_output.news_post(htlc, htlc->news_buf, dh_len);
		play_sound(snd_news);
	dh_end()
}

static void
hx_rcv_task (struct htlc_conn *htlc)
{
	struct hl_hdr *h = (struct hl_hdr *)htlc->in.buf;
	u_int32_t trans = ntohl(h->trans);
	struct task *tsk = task_with_trans(trans);

	if ((ntohl(h->flag) & 1))
		task_error(htlc);
	if (tsk) {
		/* XXX tsk->rcv might call task_delete */
		int fd = htlc->fd;
		if (tsk->rcv)
			tsk->rcv(htlc, tsk->ptr, tsk->text);
		if (hxd_files[fd].conn.htlc)
			task_delete(tsk);
	} else {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "got task 0x%08x\n", trans);
	}
}

void
hx_rcv_user_change (struct htlc_conn *htlc)
{
	u_int32_t uid = 0, cid = 0, icon = 0, color = 0;
	u_int16_t nlen = 0, got_color = 0;
	u_int8_t name[32];
	struct hx_chat *chat;
	struct hx_user *user;

	if (task_inerror(htlc))
		return;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_UID:
				dh_getint(uid);
				break;
			case HTLS_DATA_ICON:
				dh_getint(icon);
				break;
			case HTLS_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(name, dh_data, nlen);
				name[nlen] = 0;
				strip_ansi(name, nlen);
				break;
			case HTLS_DATA_COLOR:
				dh_getint(color);
				got_color = 1;
				break;
			case HTLS_DATA_CHAT_ID:
				dh_getint(cid);
				break;
		}
	dh_end()
	chat = hx_chat_with_cid(htlc, cid);
	if (!chat) {
		chat = hx_chat_new(htlc, cid);
		tty_chat_front = chat;
	}
	user = hx_user_with_uid(chat->user_list, uid);
	if (!user) {
		user = hx_user_new(&chat->user_tail);
		chat->nusers++;
		user->uid = uid;
#if defined(CONFIG_TRANSLATE)
		if (chat->translate_flags) {
			USER_TOGGLE(user, USER_FLAG_TRANSLATE);
			strcpy(user->trans_fromto, chat->translate_fromto);
			if (chat->translate_flags & 2)
				TRANS_TOGGLE(user, TRANS_FLAG_OUTPUT);
		}
#endif
		hx_output.user_create(htlc, chat, user, name, icon, color);
		if (hx_output.user_create != hx_tty_output.user_create
		    && tty_show_user_joins)
			hx_tty_output.user_create(htlc, chat, user, name, icon, color);
		play_sound(snd_join);
	} else {
		if (!got_color)
			color = user->color;
		/* always update the GUI */
		if (hx_output.user_change != hx_tty_output.user_change)
			hx_output.user_change(htlc, chat, user, name, icon, color);
		if (tty_show_user_changes && !USER_IGNORE(user)) {
			if (tty_show_user_changes == 1) {
				hx_tty_output.user_change(htlc, chat, user, name, icon, color);
			} else if (tty_show_user_changes == 2 &&
				   got_color && (user->color != color)) {
					hx_tty_output.user_change(htlc, chat, user, name, icon, color);
			}
		}
	}
	if (nlen) {
		memcpy(user->name, name, nlen);
		user->name[nlen] = 0;
	}
	if (icon)
		user->icon = icon;
	if (got_color)
		user->color = color;
	if (uid && uid == htlc->uid) {
		htlc->uid = user->uid;
		htlc->icon = user->icon;
		htlc->color = user->color;
		strcpy(htlc->name, user->name);
		hx_output.status();
	}
}

static void
hx_rcv_user_part (struct htlc_conn *htlc)
{
	u_int32_t uid = 0, cid = 0;
	struct hx_chat *chat;
	struct hx_user *user;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_UID:
				dh_getint(uid);
				break;
			case HTLS_DATA_CHAT_ID:
				dh_getint(cid);
				break;
		}
	dh_end()
	chat = hx_chat_with_cid(htlc, cid);
	if (!chat)
		return;
	user = hx_user_with_uid(chat->user_list, uid);
	if (user) {
		hx_output.user_delete(htlc, chat, user);
		if (hx_output.user_delete != hx_tty_output.user_delete
		    && tty_show_user_parts)
			hx_tty_output.user_delete(htlc, chat, user);
		hx_user_delete(&chat->user_tail, user);
		chat->nusers--;
		play_sound(snd_part);
	}
}

static void
hx_rcv_chat_subject (struct htlc_conn *htlc)
{
	u_int32_t cid = 0;
	u_int16_t slen = 0, plen = 0;
	u_int8_t *subject = 0, *password = 0;
	struct hx_chat *chat;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_CHAT_ID:
				dh_getint(cid);
				break;
			case HTLS_DATA_CHAT_SUBJECT:
				slen = dh_len >= 256 ? 255 : dh_len;
				subject = dh_data;
				break;
			case HTLS_DATA_PASSWORD:
				plen = dh_len >= 32 ? 31 : dh_len;
				password = dh_data;
				break;
		}
	dh_end()
	chat = hx_chat_with_cid(htlc, cid);
	if (!chat)
		return;
	if (subject) {
		memcpy(chat->subject, subject, slen);
		chat->subject[slen] = 0;
		hx_output.chat_subject(htlc, cid, chat->subject);
	}
	if (password) {
		memcpy(chat->password, password, plen);
		chat->password[plen] = 0;
		hx_output.chat_password(htlc, cid, chat->password);
	}
}

static void
hx_rcv_chat_invite (struct htlc_conn *htlc)
{
	u_int32_t uid = 0, cid = 0;
	u_int16_t nlen;
	u_int8_t name[32];
	struct hx_user *user = 0;
	struct hx_chat *chat = hx_chat_with_cid(htlc, 0);

	name[0] = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_UID:
				dh_getint(uid);
				break;
			case HTLS_DATA_CHAT_ID:
				dh_getint(cid);
				break;
			case HTLS_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(name, dh_data, nlen);
				name[nlen] = 0;
				strip_ansi(name, nlen);
				break;
		}
	dh_end()

	user = hx_user_with_uid(chat->user_list, uid);
	if (USER_IGNORE(user))
		return;
	play_sound(snd_chat_invite);
	hx_output.chat_invite(htlc, cid, uid, name);
}

static void
hx_rcv_user_selfinfo (struct htlc_conn *htlc)
{
	struct hl_userlist_hdr *uh;
	u_int16_t nlen;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_ACCESS:
				if (dh_len != 8)
					break;
				memcpy(&htlc->access, dh_data, 8);
				break;
			case HTLS_DATA_USER_LIST:
				if (dh_len < (SIZEOF_HL_USERLIST_HDR - SIZEOF_HL_DATA_HDR))
					break;
				uh = (struct hl_userlist_hdr *)dh;
				L16NTOH(htlc->uid, &uh->uid);
				L16NTOH(htlc->icon, &uh->icon);
				L16NTOH(htlc->color, &uh->color);
				L16NTOH(nlen, &uh->nlen);
				if (nlen > 31)
					nlen = 31;
				memcpy(htlc->name, uh->name, nlen);
				htlc->name[nlen] = 0;
				hx_output.status();
				break;
		}
	dh_end()
}

static void
hx_rcv_banner (struct htlc_conn *htlc)
{
}

#if defined(CONFIG_HLDUMP)
extern int g_hldump;
extern void hldump_packet (void *pktbuf, u_int32_t pktbuflen);

static void
hx_rcv_hldump (struct htlc_conn *htlc)
{
	if (g_hldump)
		hldump_packet(htlc->in.buf, htlc->in.pos);
	if (htlc->real_rcv)
		htlc->real_rcv(htlc);
}
#endif

extern void hx_rcv_queueupdate (struct htlc_conn *htlc);

#define MAX_HOTLINE_PACKET_LEN 0x100000

static void
hx_rcv_hdr (struct htlc_conn *htlc)
{
	struct hl_hdr *h = (struct hl_hdr *)htlc->in.buf;
	u_int32_t type, len;

	type = ntohl(h->type);
	if (ntohl(h->len) < 2)
		len = 0;
	else
		len = (ntohl(h->len) > MAX_HOTLINE_PACKET_LEN ? MAX_HOTLINE_PACKET_LEN : ntohl(h->len)) - 2;

#ifdef CONFIG_CIPHER
	if ((type >> 24) & 0xff) {
		/*hx_printf(htlc, 0, "changing decode key %u %x\n", type >> 24, type);*/
		cipher_change_decode_key(htlc, type);
		type &= 0xffffff;
	}
#endif
	/* htlc->trans = ntohl(h->trans); */
	htlc->rcv = 0;
	switch (type) {
		case HTLS_HDR_CHAT:
			htlc->rcv = hx_rcv_chat;
			break;
		case HTLS_HDR_MSG:
			htlc->rcv = hx_rcv_msg;
			break;
		case HTLS_HDR_USER_CHANGE:
		case HTLS_HDR_CHAT_USER_CHANGE:
			htlc->rcv = hx_rcv_user_change;
			break;
		case HTLS_HDR_USER_PART:
		case HTLS_HDR_CHAT_USER_PART:
			htlc->rcv = hx_rcv_user_part;
			break;
		case HTLS_HDR_NEWSFILE_POST:
			htlc->rcv = hx_rcv_news_post;
			break;
		case HTLS_HDR_TASK:
			htlc->rcv = hx_rcv_task;
			break;
		case HTLS_HDR_CHAT_SUBJECT:
			htlc->rcv = hx_rcv_chat_subject;
			break;
		case HTLS_HDR_CHAT_INVITE:
			htlc->rcv = hx_rcv_chat_invite;
			break;
		case HTLS_HDR_MSG_BROADCAST:
			hx_printf_prefix(htlc, 0, INFOPREFIX, "broadcast\n");
			htlc->rcv = hx_rcv_msg;
			break;
		case HTLS_HDR_USER_SELFINFO:
			htlc->rcv = hx_rcv_user_selfinfo;
			break;
		case HTLS_HDR_AGREEMENT:
			htlc->rcv = hx_rcv_agreement_file;
			break;
		case HTLS_HDR_POLITEQUIT:
			hx_printf_prefix(htlc, 0, INFOPREFIX, "polite quit\n");
			htlc->rcv = hx_rcv_msg;
			break;
		case HTLS_HDR_QUEUE_UPDATE:
	  		htlc->rcv = hx_rcv_queueupdate;
	  		break;
		case HTLS_HDR_BANNER:
			htlc->rcv = hx_rcv_banner;
			break;
		default:
			hx_printf_prefix(htlc, 0, INFOPREFIX, "unknown header type 0x%08x\n",
					 type);
			htlc->rcv = 0;
			break;
	}

#if defined(CONFIG_HLDUMP)
	htlc->real_rcv = htlc->rcv;
	htlc->rcv = hx_rcv_hldump;
#endif

	if (len) {
		qbuf_set(&htlc->in, htlc->in.pos, len);
	} else {
		if (htlc->rcv)
			htlc->rcv(htlc);
		htlc->rcv = hx_rcv_hdr;
		qbuf_set(&htlc->in, 0, SIZEOF_HL_HDR);
	}
}

void
hx_rcv_magic (struct htlc_conn *htlc)
{
	struct qbuf *in = &htlc->in;

	if (memcmp(in->buf, HTLS_MAGIC, HTLS_MAGIC_LEN)) {
		hx_htlc_close(htlc);
		return;
	}
	hx_connect_finish(htlc);
	htlc->rcv = hx_rcv_hdr;
	qbuf_set(in, 0, SIZEOF_HL_HDR);
}

static void
update_task (struct htlc_conn *htlc)
{
	if (htlc->in.pos >= SIZEOF_HL_HDR) {
		struct hl_hdr *h = 0;
		u_int32_t off = 0;

		/* find the last packet */
		while (off+20 <= htlc->in.pos) {
			h = (struct hl_hdr *)(&htlc->in.buf[off]);
			off += 20+ntohl(h->len);
		}
		if (h && (ntohl(h->type)&0xffffff) == HTLS_HDR_TASK) {
			struct task *tsk = task_with_trans(ntohl(h->trans));
			if (tsk) {
				tsk->pos = htlc->in.pos;
				tsk->len = htlc->in.len;
				hx_output.task_update(htlc, tsk);
			}
		}
	}
}

#define READ_BUFSIZE	0x4000
extern unsigned int decode (struct htlc_conn *htlc);

void
htlc_read (int fd)
{
	ssize_t r;
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
	struct qbuf *in = &htlc->read_in;
#if defined(__WIN32__)
	int wsaerr;
#endif

	if (!in->len) {
		qbuf_set(in, in->pos, READ_BUFSIZE);
		in->len = 0;
	}
	r = socket_read(fd, &in->buf[in->pos], READ_BUFSIZE-in->len);
	if (r <= 0) {
#if defined(__WIN32__)
		wsaerr = WSAGetLastError();
		if (r == 0 || (r < 0 && wsaerr != WSAEWOULDBLOCK && wsaerr != WSAEINTR)) {
#else
		if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EINTR)) {
#endif
			hx_printf_prefix(htlc, 0, INFOPREFIX, "htlc_read: %d %s\n", r, strerror(errno));
			hx_htlc_close(htlc);
		}
	} else {
		in->len += r;
		for (;;) {
			r = decode(htlc);
			if (!r)
				break;
			update_task(htlc);
			if (htlc->rcv) {
				if (htlc->rcv == hx_rcv_hdr) {
					hx_rcv_hdr(htlc);
					/* htlc might be destroyed in callback */
					if (!hxd_files[fd].conn.htlc)
						return;
				} else {
					htlc->rcv(htlc);
					/* htlc might be destroyed in callback */
					if (!hxd_files[fd].conn.htlc)
						return;
					goto reset;
				}
			} else {
reset:
				htlc->rcv = hx_rcv_hdr;
				qbuf_set(&htlc->in, 0, SIZEOF_HL_HDR);
			}
		}
		update_task(htlc);
	}
}

void
htlc_write (int fd)
{
	ssize_t r;
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
#if defined(__WIN32__)
	int wsaerr;
#endif

	r = socket_write(fd, &htlc->out.buf[htlc->out.pos], htlc->out.len);
	if (r <= 0) {
#if defined(__WIN32__)
		wsaerr = WSAGetLastError();
		if (r == 0 || (r < 0 && wsaerr != WSAEWOULDBLOCK && wsaerr != WSAEINTR)) {
#else
		if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EINTR)) {
#endif
			hx_printf_prefix(htlc, 0, INFOPREFIX, "htlc_write: %d %s\n", r, strerror(errno));
			hx_htlc_close(htlc);
		}
	} else {
		htlc->out.pos += r;
		htlc->out.len -= r;
		if (!htlc->out.len) {
			htlc->out.pos = 0;
			htlc->out.len = 0;
			hxd_fd_clr(fd, FDW);
		}
	}
}

void
htlc_write_connect (int s)
{
	struct htlc_conn *htlc = hxd_files[s].conn.htlc;
	int err = -1;
	socklen_t errlen = sizeof(int);

	getsockopt(s, SOL_SOCKET, SO_ERROR, (SETSOCKOPT_PTR_CAST_T)&err, &errlen);
	if (err != 0) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "connect: so_error %d\n", err);
		hx_htlc_close(htlc);
		return;
	}
	hxd_files[s].ready_write = htlc_write;
}

int
task_inerror (struct htlc_conn *htlc)
{
	struct hl_hdr *h = (struct hl_hdr *)htlc->in.buf;

	if ((ntohl(h->flag) & 1))
		return 1;

	return 0;
}
