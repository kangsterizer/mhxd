/*
 * Hotline protocol module
 */
#include "hxd.h"
#include "hlserver.h"
#include "hotline.h"
#include "rcv.h"
#include <string.h>
#include <stdio.h>
#define log_pchat 0
#define log_msg 0

static struct protocol *hotline_proto = 0;

static int
rcv_magic (struct htlc_conn *htlc)
{
	struct qbuf *in = &htlc->in;
	struct qbuf *out = &htlc->out;
	u_int32_t len;

	if (in->pos >= HTLC_MAGIC_LEN && !memcmp(htlc->in.buf, HTLC_MAGIC, HTLC_MAGIC_LEN)) {
		htlc->proto = hotline_proto;
		htlc->rcv = rcv_hdr;
		qbuf_add(out, HTLS_MAGIC, HTLS_MAGIC_LEN);
		hxd_fd_set(htlc->fd, FDW);
		len = in->pos - HTLC_MAGIC_LEN;
		/*
		 * Deal with clients that dont wait for the server magic
		 * before logging in (old hx). Only handle 1 packet.
		 */
		if (len) {
			memcpy(in->buf, in->buf+HTLC_MAGIC_LEN, len);
			if (len >= SIZEOF_HL_HDR) {
				qbuf_set(in, SIZEOF_HL_HDR, SIZEOF_HL_HDR);
				rcv_hdr(htlc);
				len -= SIZEOF_HL_HDR;
				if (len == in->len) {
					qbuf_set(in, len+SIZEOF_HL_HDR, len);
					htlc->rcv(htlc);
					htlc->rcv = rcv_hdr;
					qbuf_set(in, 0, SIZEOF_HL_HDR);
				} else {
					qbuf_set(in, len+SIZEOF_HL_HDR, in->len-len);
				}
			} else {
				qbuf_set(in, len, SIZEOF_HL_HDR);
			}
		} else {
			qbuf_set(in, 0, SIZEOF_HL_HDR);
		}
		return 1;
	}
	if (in->pos >= HTLC_MAGIC_LEN || in->buf[0] != 'T')
		return -1;

	return 0;
}

static void
snd_msg (htlc_t *htlc, htlc_t *htlcp, const u_int8_t *msg, u_int16_t msglen)
{
	u_int16_t uid16;

	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
	uid16 = htons(mangle_uid(htlc));
	hlwrite(htlcp, HTLS_HDR_MSG, 0, 3,
		HTLS_DATA_UID, sizeof(uid16), &uid16,
		HTLS_DATA_MSG, msglen, msg,
		HTLS_DATA_NAME, strlen(htlc->name), htlc->name);
	if (log_msg) {
		FILE *msglog;
		msglog = fopen("./msglog", "a");
		fprintf(msglog, "[%s]->%s: %s\n", htlc->name, htlcp->name, msg);
		fclose(msglog);
	}
}

static void
snd_chat (htlc_t *htlcp, struct htlc_chat *chat, htlc_t *htlc, const u_int8_t *buf, u_int16_t len)
{
	u_int16_t uid16;
	u_int32_t chatref, mychat;

	if (htlc)
		uid16 = htons(mangle_uid(htlc));
	else
		uid16 = 0;
	if (chat) {
		if (log_pchat)
			mychat = chatref;
		chatref = htonl(chat->ref);
		hlwrite(htlcp, HTLS_HDR_CHAT, 0, 3,
			HTLS_DATA_CHAT, len, buf,
			HTLS_DATA_CHAT_ID, sizeof(chatref), &chatref,
			HTLS_DATA_UID, sizeof(uid16), &uid16);
	} else {
		hlwrite(htlcp, HTLS_HDR_CHAT, 0, 2, 
			HTLS_DATA_CHAT, len, buf,
			HTLS_DATA_UID, sizeof(uid16), &uid16);
	}
	if (log_pchat && mychat) {
		FILE *pchatlog;
		pchatlog = fopen("./pchatlog", "a");
		fprintf(pchatlog, "[%u] %s\n", mychat, buf);
		fclose(pchatlog);
	}
}

static void
snd_error (htlc_t *htlc, const u_int8_t *str, u_int16_t len)
{
	hlwrite(htlc, HTLS_HDR_TASK, 1, 1, HTLS_DATA_TASKERROR, len, str);
}

static void
snd_user_part (struct htlc_conn *to, struct htlc_conn *parting)
{
	u_int16_t uid16;

	uid16 = htons(mangle_uid(parting));
	hlwrite(to, HTLS_HDR_USER_PART, 0, 1,
		HTLS_DATA_UID, sizeof(uid16), &uid16);
}

static void
snd_news_file (htlc_t *to, u_int8_t *buf, u_int16_t len)
{
	LF2CR(buf, len);
	hlwrite(to, HTLS_HDR_TASK, 0, 1,
		HTLS_DATA_NEWS, len, buf);
}

static void
snd_agreement_file (htlc_t *to, u_int8_t *buf, u_int16_t len)
{
	LF2CR(buf, len);
	hlwrite(to, HTLS_HDR_AGREEMENT, 0, 1,
		HTLS_DATA_AGREEMENT, len, buf);
}

static int
should_reset (htlc_t *htlc)
{
	int is_hdr = htlc->rcv == rcv_hdr;
	if (!is_hdr)
		return 1;
	else
		return 0;
}

static void
reset (htlc_t *htlc)
{
	htlc->rcv = rcv_hdr;
	qbuf_set(&htlc->in, 0, SIZEOF_HL_HDR);
}

static void
snd_user_change (htlc_t *to, htlc_t *htlc)
{
	u_int16_t uid, icon16, color;
	u_int16_t nlen;

	uid = htons(mangle_uid(htlc));
	icon16 = htons(htlc->icon);
	color = htons(htlc->color);
	nlen = strlen(htlc->name);
	if(!htlc->flags.visible)
		return;
	hlwrite(to, HTLS_HDR_USER_CHANGE, 0, 4,
		HTLS_DATA_UID, sizeof(uid), &uid,
		HTLS_DATA_ICON, sizeof(icon16), &icon16,
		HTLS_DATA_COLOR, sizeof(color), &color,
		HTLS_DATA_NAME, nlen, htlc->name);
}

struct protocol_function hotline_functions[] = {
	{ SHOULD_RESET, { should_reset } },
	{ RESET, { reset } },
	{ RCV_MAGIC, { rcv_magic } },
	{ SND_MSG, { snd_msg } },
	{ SND_CHAT, { snd_chat } },
	{ SND_ERROR, { snd_error } },
	{ SND_USER_PART, { snd_user_part } },
	{ SND_USER_CHANGE, { snd_user_change } },
	{ SND_AGREEMENT_FILE, { snd_agreement_file } },
	{ SND_NEWS_FILE, { snd_news_file } },
};
#define nhotline_functions (sizeof(hotline_functions)/sizeof(struct protocol_function))

#ifdef MODULE
#define hotline_protocol_init module_init
#endif

void
hotline_protocol_init (void)
{
	hotline_proto = protocol_register("hotline");
	if (!hotline_proto)
		return;
	protocol_register_functions(hotline_proto, hotline_functions, nhotline_functions);
}
