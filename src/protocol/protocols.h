#ifndef __hxd_protocols_h
#define __hxd_protocols_h

struct protocol_function {
	u_int32_t type;
	union {
		void *p;
		int (*should_reset)
			(htlc_t *htlc);
		void (*reset)
			(htlc_t *htlc);
		int (*rcv_magic)
			(htlc_t *from);
		void (*snd_msg)
			(htlc_t *from, htlc_t *to,
			 const u_int8_t *msg, u_int16_t msglen);
		void (*snd_chat)
			(htlc_t *to, struct htlc_chat *chat, htlc_t *from, const u_int8_t *htlc, u_int16_t chatlen);
		void (*snd_error)
			(htlc_t *to, const u_int8_t *str, u_int16_t len);
		void (*snd_user_part)
			(htlc_t *to, htlc_t *parting);
		void (*snd_user_change)
			(htlc_t *to, htlc_t *changed);
		void (*snd_news_file)
			(htlc_t *to, const u_int8_t *buf, u_int16_t len);
		void (*snd_agreement_file)
			(htlc_t *to, const u_int8_t *buf, u_int16_t len);
	} fn;
};

struct protocol_function_table {
	int (*should_reset)
		(htlc_t *htlc);
	void (*reset)
		(htlc_t *htlc);
	int (*rcv_magic)
		(htlc_t *from);
	void (*snd_msg)
		(htlc_t *from, htlc_t *to,
		 const u_int8_t *msg, u_int16_t msglen);
	void (*snd_chat)
		(htlc_t *to, struct htlc_chat *chat, htlc_t *from, const u_int8_t *htlc, u_int16_t chatlen);
	void (*snd_error)
		(htlc_t *to, const u_int8_t *chat, u_int16_t chatlen);
	void (*snd_user_part)
		(htlc_t *to, htlc_t *parting);
	void (*snd_user_change)
		(htlc_t *to, htlc_t *changed);
	void (*snd_news_file)
		(htlc_t *to, const u_int8_t *buf, u_int16_t len);
	void (*snd_agreement_file)
		(htlc_t *to, const u_int8_t *buf, u_int16_t len);
};

#define SHOULD_RESET		0x00001
#define RESET			0x00002

#define RCV_MAGIC		0x10001

#define SND_MSG			0x20001
#define SND_CHAT		0x20002
#define SND_ERROR		0x20003
#define SND_USER_PART		0x20100
#define SND_USER_CHANGE		0x20101
#define SND_NEWS_FILE		0x20200
#define SND_AGREEMENT_FILE	0x20201

struct protocol {
	struct protocol *next;
	char name[32];
	u_int32_t proto_id;
	struct protocol_function_table ftab;
};

extern void protocol_register_function (struct protocol *proto, struct protocol_function *pf);
extern void protocol_register_functions (struct protocol *proto, struct protocol_function *pf, unsigned int npfs);
extern struct protocol *protocol_register (const char *name);
extern void protocol_deregister (const char *name);
extern void protocol_rcv_magic (htlc_t *htlc);

#endif
