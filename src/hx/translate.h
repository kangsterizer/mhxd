#ifndef __hx_translate_h
#define __hx_translate_h

struct trans_conn {
	struct htlc_conn *htlc;
	char *text;
	char *buf;
	char name[32];
	char fromto[8];
	u_int32_t buflen;
	u_int32_t cid;
	u_int32_t uid;
	unsigned int translate_flags;
	int s;
	int type;
};

#define TRANS_FLAG_CONNECTED 1
#define TRANS_FLAG_OUTPUT 2
#define TRANS_CONNECTED(_t) ((_t)->translate_flags & TRANS_FLAG_CONNECTED)
#define TRANS_OUTPUT(_t) ((_t)->translate_flags & TRANS_FLAG_OUTPUT)
#define TRANS_TOGGLE(_t, _f) ((_t)->translate_flags ^= (_f))

#endif /* __hx_translate_h */
