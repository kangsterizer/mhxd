#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include "sys_net.h"
#include "hxd.h"
#include "xmalloc.h"

void
hlwrite (struct htlc_conn *htlc, u_int32_t type, u_int32_t flag, int hc, ...)
{
	va_list ap;
	struct hl_hdr h;
	struct hl_data_hdr dhs;
	struct qbuf *q;
	u_int32_t this_off, pos, len;

	if (!htlc->fd && !htlc->wfd)
		return;

	q = &htlc->out;
	this_off = q->pos + q->len;
	pos = this_off + SIZEOF_HL_HDR;
	q->len += SIZEOF_HL_HDR;
	q->buf = xrealloc(q->buf, q->pos + q->len);

	h.type = htonl(type);
	if (type == HTLS_HDR_TASK) {
		h.trans = htonl(htlc->replytrans);
	} else {
		h.trans = htonl(htlc->trans);
		htlc->trans++;
	}
	h.flag = htonl(flag);
	h.hc = htons(hc);

	va_start(ap, hc);
	while (hc) {
		u_int16_t t, l;
		u_int8_t *data;

		t = (u_int16_t)va_arg(ap, int);
		l = (u_int16_t)va_arg(ap, int);
		dhs.type = htons(t);
		dhs.len = htons(l);

		q->len += SIZEOF_HL_DATA_HDR + l;
		q->buf = xrealloc(q->buf, q->pos + q->len);
		memory_copy(&q->buf[pos], (u_int8_t *)&dhs, 4);
		pos += 4;
		data = va_arg(ap, u_int8_t *);
		if (l) {
			memory_copy(&q->buf[pos], data, l);
			pos += l;
		}
		hc--;
	}
	va_end(ap);

	len = pos - this_off;
	h.len = htonl(len - (SIZEOF_HL_HDR - sizeof(h.hc)));
	h.totlen = h.len;

	memory_copy(q->buf + this_off, &h, SIZEOF_HL_HDR);
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE)
		len = compress_encode(htlc, this_off, len);
#endif
#ifdef CONFIG_CIPHER
	if (htlc->cipher_encode_type != CIPHER_NONE)
		cipher_encode(htlc, this_off, len);
#endif

	if (htlc->wfd)
		hxd_fd_set(htlc->wfd, FDW);
	else
		hxd_fd_set(htlc->fd, FDW);
}

u_int32_t
hlwrite_hdr (struct htlc_conn *htlc, u_int32_t type, u_int32_t flag)
{
	u_int32_t hoff;
	struct qbuf *q;
	struct hl_hdr h;

	q = &htlc->out;
	hoff = q->pos + q->len;
	q->len += SIZEOF_HL_HDR;
	q->buf = xrealloc(q->buf, q->pos + q->len);

	h.type = htonl(type);
	if (type == HTLS_HDR_TASK) {
		h.trans = htonl(htlc->replytrans);
	} else {
		h.trans = htonl(htlc->trans);
		htlc->trans++;
	}
	h.flag = htonl(flag);
	h.flag = htons(flag);
	h.len = h.totlen = htonl(2);
	h.hc = 0;
	memory_copy(q->buf + hoff, &h, SIZEOF_HL_HDR);

	return hoff;
}

void
hlwrite_data (struct htlc_conn *htlc, u_int32_t hoff, u_int16_t type, u_int16_t len, void *data)
{
	struct qbuf *q;
	struct hl_data_hdr dh;
	u_int16_t hc;
	u_int32_t off, totlen;

	q = &htlc->out;
	q->len += SIZEOF_HL_DATA_HDR + len;
	q->buf = xrealloc(q->buf, q->pos + q->len);

	L32NTOH(totlen, q->buf + hoff + 12);
	off = hoff + SIZEOF_HL_HDR-2 + totlen;
	totlen += SIZEOF_HL_DATA_HDR + len;
	dh.type = htons(type);
	dh.len = htons(len);
	memory_copy(q->buf + off, &dh, SIZEOF_HL_DATA_HDR);
	off += SIZEOF_HL_DATA_HDR;
	memory_copy(q->buf + off, data, len);
	off += len;

	/* update the packet header */
	L16NTOH(hc, q->buf + hoff + 20);
	hc++;
	S16HTON(hc, q->buf + hoff + 20);
	S32HTON(totlen, q->buf + hoff + 16);
	S32HTON(totlen, q->buf + hoff + 12);
}

void
hlwrite_end (struct htlc_conn *htlc, u_int32_t hoff)
{
	u_int32_t len;
	struct qbuf *q;

	q = &htlc->out;
	L32NTOH(len, q->buf + hoff + 12);
	len += SIZEOF_HL_HDR-2;
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE)
		len = compress_encode(htlc, hoff, len);
#endif
#ifdef CONFIG_CIPHER
	if (htlc->cipher_encode_type != CIPHER_NONE)
		cipher_encode(htlc, hoff, len);
#endif
	hxd_fd_set(htlc->fd, FDW);
}

void
hlwrite_dhdrs (struct htlc_conn *htlc, u_int16_t dhdrcount, struct hl_data_hdr **dhdrs)
{
	u_int32_t hoff;
	u_int16_t i;

	hoff = hlwrite_hdr(htlc, HTLS_HDR_TASK, 0);
	for (i = 0; i < dhdrcount; i++)
		hlwrite_data(htlc, hoff, dhdrs[i]->type, dhdrs[i]->len,
				((u_int8_t *)dhdrs[i])+SIZEOF_HL_DATA_HDR);
	hlwrite_end(htlc, hoff);
}

/* This is used to encode/decode passwords */
void
hl_code (void *__dst, const void *__src, size_t len)
{
	u_int8_t *dst = (u_int8_t *)__dst, *src = (u_int8_t *)__src;

	for (; len; len--)
		*dst++ = ~*src++;
}
