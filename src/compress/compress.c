#include <string.h>
#include "hxd.h"
#include "compress.h"
#include "xmalloc.h"

#define COMPRESS_DEBUG	0

#if COMPRESS_DEBUG
#include <stdio.h>
#include <unistd.h>

static void
writestuff (const char *str, u_int8_t type, const u_int8_t *buf, unsigned int len)
{
	unsigned int i;
	char file[32];
	FILE *fp;

	sprintf(file, "/tmp/compress.%d", getpid());
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

static int level = 6; /* Z_DEFAULT_COMPRESSION; */
static int flush = Z_SYNC_FLUSH;

u_int32_t
compress_decode (struct htlc_conn *htlc, struct qbuf *out, struct qbuf *in,
		 u_int32_t max, u_int32_t *inusedp)
{
	z_stream *stream = &htlc->compress_decode_state.stream;
	unsigned long dstlen, pos;
	u_int32_t used;
	int err;

#if COMPRESS_DEBUG
	writestuff("src: ", 0, &in->buf[in->pos], in->len);
#endif
	dstlen = max;
	pos = 0;
	qbuf_set(out, out->pos, max);
	stream->next_in = &in->buf[in->pos];
	stream->avail_in = in->len;

	for (;;) {
		stream->next_out = &out->buf[out->pos+pos];
		stream->avail_out = dstlen;
		err = inflate(stream, flush);
		pos += dstlen - stream->avail_out;
		dstlen -= dstlen - stream->avail_out;
		if (err != Z_OK)
			break;
	}
	dstlen = max - dstlen;
	used = in->len - stream->avail_in;
#if COMPRESS_DEBUG
	if (err != Z_OK)
		hxd_log("err=%d msg=%s",err,stream->msg);
	writestuff("dec: ", err, &out->buf[out->pos], dstlen);
	hxd_log("in: %d out: %d", used, dstlen);
#endif
	htlc->gzip_inflate_total_in += used;
	htlc->gzip_inflate_total_out += dstlen;
	*inusedp = used;

	return dstlen;
}

#define ZLIB_MAX_INPUT		0x1000
#define ZLIB_OUT_BUFSIZE	(ZLIB_MAX_INPUT+ZLIB_MAX_INPUT/1000+13)

static u_int32_t
do_encode (struct htlc_conn *htlc, u_int32_t pos, u_int32_t len)
{
	z_stream *stream = &htlc->compress_encode_state.stream;
	u_int8_t *buf, autobuf[ZLIB_OUT_BUFSIZE];
	u_int32_t dstlen, inpos, thislen;
	int err;

	buf = autobuf;
	inpos = 0;
	dstlen = 0;
	for (;;) {
		stream->next_in = &htlc->out.buf[pos+inpos];
		thislen = len - inpos;
		if (thislen > ZLIB_MAX_INPUT)
			thislen = ZLIB_MAX_INPUT;
		stream->avail_in = thislen;
		stream->next_out = &buf[dstlen];
		stream->avail_out = ZLIB_OUT_BUFSIZE;
		err = deflate(stream, flush);
		if (err == Z_OK) {
			dstlen += ZLIB_OUT_BUFSIZE - stream->avail_out;
			inpos += thislen - stream->avail_in;
			if (len == inpos)
				break;
			if (buf == autobuf) {
				buf = xmalloc(dstlen + ZLIB_OUT_BUFSIZE);
				memcpy(buf, autobuf, dstlen);
			} else
				buf = xrealloc(buf, dstlen + ZLIB_OUT_BUFSIZE);
		} else
			break;
	}
	htlc->out.len -= len;
	qbuf_set(&htlc->out, htlc->out.pos, htlc->out.len + dstlen);
	memcpy(&htlc->out.buf[pos], buf, dstlen);
	htlc->gzip_deflate_total_in += len;
	htlc->gzip_deflate_total_out += dstlen;
#if COMPRESS_DEBUG
	writestuff("enc: ", err, &htlc->out.buf[pos], dstlen);
#endif
	if (buf != autobuf)
		xfree(buf);

	return dstlen;
}

u_int32_t
compress_encode (struct htlc_conn *htlc, u_int32_t pos, u_int32_t len)
{
#if defined(CONFIG_CIPHER) && 0
	if (htlc->cipher_encode_type != CIPHER_NONE) {
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
			htlc->zc_ran = ran;
			htlc->zc_hdrlen = do_encode(htlc, pos, SIZEOF_HL_HDR);
			pos += SIZEOF_HL_HDR;
			len -= SIZEOF_HL_HDR;

			return (htlc->zc_hdrlen + do_encode(htlc, pos, len));
		}
	}
#endif

	return do_encode(htlc, pos, len);
}

void
compress_encode_init (struct htlc_conn *htlc)
{
	z_stream *stream = &htlc->compress_encode_state.stream;
	int err;

	stream->zalloc = 0;
	stream->zfree = 0;
	stream->opaque = 0;
	err = deflateInit(stream, level);
#if COMPRESS_DEBUG
	writestuff("di: ", err, 0, 0);
#endif
}

void
compress_decode_init (struct htlc_conn *htlc)
{
	z_stream *stream = &htlc->compress_decode_state.stream;
	int err;

	if (htlc->compress_decode_type != COMPRESS_GZIP)
		return;
	stream->zalloc = 0;
	stream->zfree = 0;
	err = inflateInit(stream);
#if COMPRESS_DEBUG
	writestuff("ii: ", err, 0, 0);
#endif
}

void
compress_encode_end (struct htlc_conn *htlc)
{
	z_stream *stream = &htlc->compress_encode_state.stream;

	if (htlc->compress_encode_type != COMPRESS_GZIP)
		return;
	deflateEnd(stream);
}

void
compress_decode_end (struct htlc_conn *htlc)
{
	z_stream *stream = &htlc->compress_decode_state.stream;

	if (htlc->compress_decode_type != COMPRESS_GZIP)
		return;
	inflateEnd(stream);
}
