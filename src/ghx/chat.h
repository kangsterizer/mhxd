#ifndef __hx_chat_h
#define __hx_chat_h

extern void chat_subject (struct htlc_conn *htlc, u_int32_t cid, const char *subject);
extern void chat_password (struct htlc_conn *htlc, u_int32_t cid, const u_int8_t *pass);
extern void chat_invite (struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid, const char *name);
extern void chat_delete (struct htlc_conn *htlc, struct hx_chat *chat);

#endif /* __hx_chat_h */
