#include "hxd.h"
#include "hx.h"
#include "xmalloc.h"
#include <string.h>

struct hx_chat *
hx_chat_new (struct htlc_conn *htlc, u_int32_t cid)
{
	struct hx_chat *chat;

	chat = xmalloc(sizeof(struct hx_chat));
	memset(chat, 0, sizeof(struct hx_chat));
	chat->cid = cid;
	chat->user_list = &chat->__user_list;
	chat->user_tail = &chat->__user_list;

	chat->next = 0;
	chat->prev = htlc->chat_list;
	if (htlc->chat_list)
		htlc->chat_list->next = chat;
	htlc->chat_list = chat;

	return chat;
}

void
hx_chat_delete (struct htlc_conn *htlc, struct hx_chat *chat)
{
	hx_output.chat_delete(htlc, chat);
	if (chat->next)
		chat->next->prev = chat->prev;
	if (chat->prev)
		chat->prev->next = chat->next;
	if (htlc->chat_list == chat)
		htlc->chat_list = chat->prev;
	xfree(chat);
}

struct hx_chat *
hx_chat_with_cid (struct htlc_conn *htlc, u_int32_t cid)
{
	struct hx_chat *chatp;

	for (chatp = htlc->chat_list; chatp; chatp = chatp->prev)
		if (chatp->cid == cid)
			return chatp;

	return 0;
}
