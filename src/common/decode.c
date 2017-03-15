#include <string.h>
#include "hxd.h"
#include "xmalloc.h"

unsigned int
decode (struct htlc_conn *htlc)
{
	struct qbuf *in;
	struct qbuf *out;
	u_int32_t len, max, inused, inlen;
#ifdef CONFIG_CIPHER
	union cipher_state cipher_state;
	struct qbuf cipher_out;
#endif
#ifdef CONFIG_COMPRESS
	struct qbuf compress_out;
#endif

#ifdef CONFIG_CIPHER
	memset(&cipher_out, 0, sizeof(struct qbuf));
#endif
#ifdef CONFIG_COMPRESS
	memset(&compress_out, 0, sizeof(struct qbuf));
#endif

	in = &htlc->read_in;
	out = in;
	inlen = in->len;
	if (!inlen)
		return 0;
	inused = 0;
	len = inlen;
	in->pos = 0;

#ifdef CONFIG_CIPHER
#ifdef CONFIG_COMPRESS
	if (htlc->compress_decode_type != COMPRESS_NONE)
		max = 0xffffffff;
	else
#endif
		max = htlc->in.len;
	if (htlc->cipher_decode_type != CIPHER_NONE) {
		memcpy(&cipher_state, &htlc->cipher_decode_state, sizeof(cipher_state));
		out = &cipher_out;
		len = cipher_decode(htlc, out, in, max, &inused);
	} else
#endif
#ifdef CONFIG_COMPRESS
	if (htlc->compress_decode_type == COMPRESS_NONE) {
#endif
		max = htlc->in.len;
		out = in;
		if (max && inlen > max) {
			inused = max;
			len = max;
		} else {
			inused = inlen;
			len = inlen;
		}
#ifdef CONFIG_COMPRESS
	}

	if (htlc->compress_decode_type != COMPRESS_NONE) {
		max = htlc->in.len;
		out = &compress_out;
		len = compress_decode(htlc, out,
#ifdef CONFIG_CIPHER
				      htlc->cipher_decode_type == CIPHER_NONE ? in : &cipher_out,
#else
				      in,
#endif
				      max, &inused);
	}
#endif
	if (htlc->in.len < len)
		qbuf_set(&htlc->in, htlc->in.pos, len);
	memcpy(&htlc->in.buf[htlc->in.pos], &out->buf[out->pos], len);
	if (inlen != inused) {
#ifdef CONFIG_CIPHER
		if (htlc->cipher_decode_type != CIPHER_NONE) {
			memcpy(&htlc->cipher_decode_state, &cipher_state, sizeof(cipher_state));
			cipher_decode(htlc, &cipher_out, in, inused, &inused);
		}
#endif
		memmove(&in->buf[0], &in->buf[inused], inlen - inused);
	}
	in->pos = inlen - inused;
	in->len -= inused;
	htlc->in.pos += len;
	if (len > htlc->in.len) {
		/* More data than expected */
		htlc->in.len = 0;
	} else
		htlc->in.len -= len;

#if defined(CONFIG_COMPRESS)
	if (compress_out.buf)
		xfree(compress_out.buf);
#endif
#if defined(CONFIG_CIPHER)
	if (cipher_out.buf)
		xfree(cipher_out.buf);
#endif

	return (htlc->in.len == 0) ? 1 : 0;
}
