#ifndef __hxd_snd_h
#define __hxd_snd_h

extern void snd_errorstr (struct htlc_conn *htlc, const u_int8_t *str);
extern void snd_strerror (struct htlc_conn *htlc, int err);
extern void snd_user_part (struct htlc_conn *htlc);
extern void snd_user_join (struct htlc_conn *htlc);
extern void snd_user_change (struct htlc_conn *htlc);
extern void snd_news_file (htlc_t *to, u_int8_t *buf, u_int16_t len);
extern void snd_agreement_file (htlc_t *to, u_int8_t *buf, u_int16_t len);
extern void snd_chat_toone (htlc_t *to, struct htlc_chat *chat, htlc_t *from, u_int8_t *buf, u_int16_t len);
extern void snd_chat (struct htlc_chat *chat, struct htlc_conn *htlc, u_int8_t *buf, u_int16_t len);
extern void snd_msg (struct htlc_conn *htlc, struct htlc_conn *htlcp, u_int8_t *msg, u_int16_t msglen);

#endif
