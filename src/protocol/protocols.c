/*
 * Protocol module interface.
 */
#include "hxd.h"
#include "xmalloc.h"
#include <string.h>

static struct protocol *proto_list = 0, *proto_tail = 0;

void
protocol_rcv_magic (htlc_t *htlc)
{
	struct protocol *proto;
	int matched, ok = 0;

	for (proto = proto_list; proto; proto = proto->next) {
		if (!proto->ftab.rcv_magic)
			continue;
		matched = proto->ftab.rcv_magic(htlc);
		if (matched != -1)
			ok = 1;
		if (matched == 1)
			break;
	}
	/*
	 * If each rcv_magic returned -1,
	 * then the protocol is not supported.
	 */
	if (!ok)
		htlc_close(htlc);
}

#define set(x) proto->ftab.x = pf->fn.x

void
protocol_register_function (struct protocol *proto, struct protocol_function *pf)
{
	switch (pf->type) {
		case SHOULD_RESET:
			set(should_reset);
			break;
		case RESET:
			set(reset);
			break;
		case RCV_MAGIC:
			set(rcv_magic);
			break;
		case SND_MSG:
			set(snd_msg);
			break;
		case SND_CHAT:
			set(snd_chat);
			break;
		case SND_ERROR:
			set(snd_error);
			break;
		case SND_USER_PART:
			set(snd_user_part);
			break;
		case SND_USER_CHANGE:
			set(snd_user_change);
			break;
		case SND_NEWS_FILE:
			set(snd_news_file);
			break;
		case SND_AGREEMENT_FILE:
			set(snd_agreement_file);
			break;
		default:
			break;
	}
}

void
protocol_register_functions (struct protocol *proto, struct protocol_function *pf, unsigned int npfs)
{
	while (npfs) {
		protocol_register_function(proto, pf);
		pf++;
		npfs--;
	}
}

struct protocol *
protocol_get (const char *name)
{
	struct protocol *proto;

	for (proto = proto_list; proto; proto = proto->next) {
		if (!strcmp(proto->name, name))
			return proto;
	}

	return proto;
}

struct protocol *
protocol_new (const char *name)
{
	struct protocol *proto;

	proto = xmalloc(sizeof(struct protocol));
	proto->next = 0;
	strlcpy(proto->name, name, sizeof(proto->name));
	memset(&proto->ftab, 0, sizeof(proto->ftab));
	if (!proto_list)
		proto_list = proto;
	else
		proto_tail->next = proto;
	proto_tail = proto;

	return proto;
}

void
protocol_del (struct protocol *proto)
{
	xfree(proto);
	if (proto_list == proto)
		proto_list = proto_list->next;
	if (proto == proto_tail) {
		if (!proto_list) {
			proto_tail = 0;
		} else {
			struct protocol *p;
			for (p = proto_list; p->next; p = p->next)
				;
			proto_tail = p;
		}
	}
}

static u_int32_t proto_id = 0;

static u_int32_t
proto_id_new (void)
{
	proto_id++;
	return proto_id;
}

struct protocol *
protocol_register (const char *name)
{
	struct protocol *proto;

	proto = protocol_get(name);
	if (!proto)
		proto = protocol_new(name);
	proto->proto_id = proto_id_new();

	return proto;
}

void
protocol_unregister (const char *name)
{
	struct protocol *proto;

	proto = protocol_get(name);
	if (!proto)
		return;
	protocol_del(proto);
}
