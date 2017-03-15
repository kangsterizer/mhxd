/*
 * Protocol independent send functions.
 * TODO: make inline in snd.h
 */
#include "hxd.h"
#include "snd.h"
#include "hlserver.h"
#include <string.h>

extern struct htlc_conn *htlc_list;

#define proto_call(_htlc, _fn) if (_htlc->proto) if (_htlc->proto->ftab._fn) _htlc->proto->ftab._fn

void
snd_user_join (htlc_t *htlc)
{
	htlc_t *htlcp;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp == htlc || !htlcp->access_extra.user_getlist)
			continue;
		proto_call(htlcp, snd_user_change)(htlcp, htlc);
	}
}

void
snd_user_change (htlc_t *htlc)
{
	htlc_t *htlcp;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!htlcp->access_extra.user_getlist)
			continue;
		proto_call(htlcp, snd_user_change)(htlcp, htlc);
	}
}

void
snd_errorstr (struct htlc_conn *htlc, const u_int8_t *str)
{
	u_int16_t len = strlen(str);

	if (!htlc->proto)
		return;
	proto_call(htlc, snd_error)(htlc, str, len);
}

void
snd_strerror (struct htlc_conn *htlc, int err)
{
	char *str = strerror(err);

	snd_errorstr(htlc, str);
}

void
snd_user_part (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!htlcp->access_extra.user_getlist)
			continue;
		proto_call(htlc, snd_user_part)(htlcp, htlc);
	}
}

void
snd_news_file (htlc_t *to, u_int8_t *buf, u_int16_t len)
{
	proto_call(to, snd_news_file)(to, buf, len);
}

void
snd_agreement_file (htlc_t *to, u_int8_t *buf, u_int16_t len)
{
	proto_call(to, snd_agreement_file)(to, buf, len);
}

void
snd_msg (struct htlc_conn *htlc, struct htlc_conn *htlcp, u_int8_t *msg, u_int16_t msglen)
{
	proto_call(htlc, snd_msg)(htlc, htlcp, msg, msglen);
}

void
snd_chat (struct htlc_chat *chat, struct htlc_conn *htlc, u_int8_t *buf, u_int16_t len)
{
	struct htlc_conn *htlcp;

	if (chat) {
		for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
			if (!chat_isset(htlcp, chat, 0))
				continue;
			proto_call(htlcp, snd_chat)(htlcp, chat, htlc, buf, len);
		}
	} else {
		for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
			if (!htlcp->access.read_chat)
				continue;
			proto_call(htlcp, snd_chat)(htlcp, chat, htlc, buf, len);
		}
	}
}

void
snd_chat_toone (htlc_t *to, struct htlc_chat *chat, htlc_t *from, u_int8_t *buf, u_int16_t len)
{
	proto_call(to, snd_chat)(to, chat, from, buf, len);
}
