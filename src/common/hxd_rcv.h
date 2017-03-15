#ifndef __hxd_hxd_rcv_h
#define __hxd_hxd_rcv_h

#include "hxd.h"

extern void hxd_rcv_chat (struct htlc_conn *htlc, struct htlc_chat *chat,
			  u_int16_t style, u_int16_t away, u_int8_t *chatbuf, u_int16_t len);
extern void hxd_rcv_msg (struct htlc_conn *htlc, struct htlc_conn *htlcp, u_int8_t *msg, u_int16_t msglen);

#endif
