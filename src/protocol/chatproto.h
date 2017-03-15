#ifndef __chatproto_h
#define __chatproto_h

extern void cp_rcv_chat (struct htlc_conn *htlc);

extern void cp_snd_chat (struct htlc_conn *htlc, u_int8_t *buf, u_int16_t len);
extern void cp_snd_msg (struct htlc_conn *htlc, struct htlc_conn *htlcp, u_int8_t *buf, u_int16_t len);
extern void cp_snd_user_part (struct htlc_conn *htlcp, struct htlc_conn *htlc);
extern void cp_snd_error (struct htlc_conn *htlc, const u_int8_t *str, u_int16_t len);

#endif
