#include <iconv.h>
#include "xmalloc.h"

#define SIZEOF_WCHAR	4

size_t
convbuf (const char *to, const char *from, const char *inbuf, size_t inlen, char **outbufp)
{
	char *inp, *outp, *outbuf;
	size_t outlen, outleft, ir, insize;
	iconv_t d;

	d = iconv_open(to, from);
	if (!d) {
		*outbufp = 0;
		return 0;
	}
	inp = (char *)inbuf;
	outlen = inlen*SIZEOF_WCHAR;
	outbuf = xmalloc(outlen+1);
	outleft = outlen;
	insize = inlen;
	for (outp = outbuf, inp = (char *)inbuf; inp < inbuf+inlen; ) {
		ir = iconv(d, &inp, &insize, &outp, &outleft);
		if (ir == (size_t)-1) {
			inp++;
			insize--;
			continue;
		}
		if (inlen == 0)
			break;
	}
	outlen -= outleft;
	outp = outp - outlen;
	*outbufp = outp;
	iconv_close(d);
	outp[outlen] = 0;

	return outlen;
}
