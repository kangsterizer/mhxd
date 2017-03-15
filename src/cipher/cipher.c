#include "hxd.h"
#include "cipher.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define CIPHER_DEBUG	0

#if CIPHER_DEBUG
#include <stdio.h>

static void
writestuff (const char *str, u_int8_t type, const u_int8_t *buf, unsigned int len)
{
	unsigned int i;
	char file[32];
	FILE *fp;

	sprintf(file, "/tmp/cipher.%d", getpid());
	fp = fopen(file, "a");
	if (!fp)
		return;
	fprintf(fp, str);
	fprintf(fp, "%u: ", type);
	for (i = 0; i < len; i++)
		fprintf(fp, "%2.2x", buf[i]);
	fprintf(fp, "\n");
	fflush(fp);
	fclose(fp);
}
#endif

u_int32_t
cipher_decode (struct htlc_conn *htlc, struct qbuf *out, struct qbuf *in,
	       u_int32_t max, u_int32_t *inusedp)
{
	u_int32_t len;

	len = in->len;
	if (len > max)
		len = max;
	qbuf_set(out, 0, len);
#if CIPHER_DEBUG
	writestuff("dec: ", htlc->cipher_decode_type, &in->buf[in->pos], len);
#endif
	switch (htlc->cipher_decode_type) {
		case CIPHER_RC4:
			memcpy(&out->buf[out->pos], &in->buf[in->pos], len);
			rc4_buffer(&out->buf[out->pos], len,
				   &htlc->cipher_decode_state.rc4);
			break;
		case CIPHER_BLOWFISH:
			BF_ofb64_encrypt(&in->buf[in->pos],
					 &out->buf[out->pos], len,
					 &htlc->cipher_decode_state.blowfish.state,
					 htlc->cipher_decode_state.blowfish.ivec,
					 &htlc->cipher_decode_state.blowfish.num);
			break;
#if !defined(CONFIG_NO_IDEA)
		case CIPHER_IDEA:
			idea_ofb64_encrypt(&in->buf[in->pos],
					   &out->buf[out->pos], len,
					   &htlc->cipher_decode_state.idea.state,
					   htlc->cipher_decode_state.idea.ivec,
					   &htlc->cipher_decode_state.idea.num);
			break;
#endif
		default:
			break;
	}
#if CIPHER_DEBUG
	writestuff("dec: ", htlc->cipher_decode_type, &out->buf[out->pos], len);
#endif
	*inusedp = len;

	return len;
}

void
cipher_change_decode_key (struct htlc_conn *htlc, u_int32_t type)
{
	u_int16_t len = 0;
	u_int32_t i, num;

	num = (type >> 24) & 0xff;
	for (i = 0; i < num; i++) {
		len = hmac_xxx(htlc->cipher_decode_key,
			       htlc->cipher_decode_key, htlc->cipher_decode_keylen,
			       htlc->sessionkey, htlc->sklen, htlc->macalg);
	}
#if CIPHER_DEBUG
	writestuff("deckey: ", num, htlc->cipher_decode_key, len);
#endif
	htlc->cipher_decode_keylen = len;
	cipher_decode_init(htlc);
}

static void
cipher_change_encode_key (struct htlc_conn *htlc, unsigned int num)
{
	u_int16_t len = 0;
	unsigned int i;

	for (i = 0; i < num; i++) {
		len = hmac_xxx(htlc->cipher_encode_key,
			       htlc->cipher_encode_key, htlc->cipher_encode_keylen,
			       htlc->sessionkey, htlc->sklen, htlc->macalg);
	}
#if CIPHER_DEBUG
	writestuff("enckey: ", num, htlc->cipher_encode_key, len);
#endif
	htlc->cipher_encode_keylen = len;
	cipher_encode_init(htlc);
}

static void
do_encode (struct htlc_conn *htlc, unsigned int pos, unsigned int len)
{
#if CIPHER_DEBUG
	writestuff("enc: ", htlc->cipher_encode_type, &htlc->out.buf[pos], len);
#endif
	switch (htlc->cipher_encode_type) {
		case CIPHER_RC4:
			rc4_buffer(&htlc->out.buf[pos], len,
				   &htlc->cipher_encode_state.rc4);
			break;
		case CIPHER_BLOWFISH:
			BF_ofb64_encrypt(&htlc->out.buf[pos],
					 &htlc->out.buf[pos], len,
					 &htlc->cipher_encode_state.blowfish.state,
					 htlc->cipher_encode_state.blowfish.ivec,
					 &htlc->cipher_encode_state.blowfish.num);
			break;
#if !defined(CONFIG_NO_IDEA)
		case CIPHER_IDEA:
			idea_ofb64_encrypt(&htlc->out.buf[pos],
					   &htlc->out.buf[pos], len,
					   &htlc->cipher_encode_state.idea.state,
					   htlc->cipher_encode_state.idea.ivec,
					   &htlc->cipher_encode_state.idea.num);
			break;
#endif
		default:
			break;
	}
#if CIPHER_DEBUG
	writestuff("enc: ", htlc->cipher_encode_type, &htlc->out.buf[pos], len);
#endif
}

void
cipher_encode (struct htlc_conn *htlc, unsigned int pos, unsigned int len)
{
	if (htlc->cipher_encode_type != CIPHER_NONE) {
#if defined(CONFIG_COMPRESS)
		if (htlc->compress_encode_type == COMPRESS_NONE) {
#endif
			unsigned char ran;

			random_bytes(&ran, 1);
			ran >>= 4;
			if (ran == 2 || ran == 7 || ran == 13) {
				u_int32_t type;

				random_bytes(&ran, 1);
				ran >>= 2;
				if (!ran) {
					random_bytes(&ran, 1);
					ran = (ran >> 3) + 1;
				}
				L32NTOH(type, &htlc->out.buf[pos]);
				type |= (((u_int32_t)ran << 24) & 0xff000000);
				S32HTON(type, &htlc->out.buf[pos]);
				do_encode(htlc, pos, SIZEOF_HL_HDR);
				cipher_change_encode_key(htlc, ran);
				pos += SIZEOF_HL_HDR;
				len -= SIZEOF_HL_HDR;
			}
#if defined(CONFIG_COMPRESS)
		} else if (htlc->zc_ran) {
			do_encode(htlc, pos, htlc->zc_hdrlen);
			cipher_change_encode_key(htlc, htlc->zc_ran);
			pos += htlc->zc_hdrlen;
			len -= htlc->zc_hdrlen;
			htlc->zc_ran = 0;
		}
#endif
	} else {
		return;
	}
	do_encode(htlc, pos, len);
}

void
cipher_encode_init (struct htlc_conn *htlc)
{
	switch (htlc->cipher_encode_type) {
		case CIPHER_RC4:
			rc4_prepare_key(htlc->cipher_encode_key, htlc->cipher_encode_keylen,
					&htlc->cipher_encode_state.rc4);
			break;
		case CIPHER_BLOWFISH:
			BF_set_key(&htlc->cipher_encode_state.blowfish.state,
				   htlc->cipher_encode_keylen, htlc->cipher_encode_key);
			break;
#if !defined(CONFIG_NO_IDEA)
		case CIPHER_IDEA:
			idea_set_encrypt_key(htlc->cipher_encode_key,
					     &htlc->cipher_encode_state.idea.state);
			break;
#endif
		default:
			break;
	}
}

void
cipher_decode_init (struct htlc_conn *htlc)
{
	switch (htlc->cipher_decode_type) {
		case CIPHER_RC4:
			rc4_prepare_key(htlc->cipher_decode_key, htlc->cipher_decode_keylen,
					&htlc->cipher_decode_state.rc4);
			break;
		case CIPHER_BLOWFISH:
			BF_set_key(&htlc->cipher_decode_state.blowfish.state,
				   htlc->cipher_decode_keylen, htlc->cipher_decode_key);
			break;
#if !defined(CONFIG_NO_IDEA)
		case CIPHER_IDEA:
			idea_set_encrypt_key(htlc->cipher_decode_key,
					     &htlc->cipher_decode_state.idea.state);
			break;
#endif
		default:
			break;
	}
}
