#ifndef __compress_h
#define __compress_h

#include <zlib.h>

#define COMPRESS_NONE	0
#define COMPRESS_GZIP	1

union compress_state {
	z_stream stream;
};

struct htlc_conn;
struct qbuf;

extern u_int32_t compress_decode (struct htlc_conn *htlc, struct qbuf *out, struct qbuf *in, u_int32_t max, u_int32_t *inusedp);
extern u_int32_t compress_encode (struct htlc_conn *htlc, u_int32_t pos, u_int32_t len);
extern void compress_encode_init (struct htlc_conn *htlc);
extern void compress_decode_init (struct htlc_conn *htlc);
extern void compress_encode_end (struct htlc_conn *htlc);
extern void compress_decode_end (struct htlc_conn *htlc);

#endif
