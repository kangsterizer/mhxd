#include <string.h>
#include <unistd.h>
#include "xmalloc.h"
#include "getline.h"

char *
getline_line (getline_t *gl, int *linelenp)
{
	int fd = gl->fd;
	char *buf = gl->buf;
	char *line, *end, *p;
	int pos = gl->pos;
	int len = gl->len;
	int size = gl->size;
	int r;

	end = buf+size;
	if (pos == 0) {
		r = read(fd, buf+len, size - len);
		if (r == 0) {
			*linelenp = -1;
			return 0;
		} else if (r < 0) {
			*linelenp = -2;
			return 0;
		}
	}
	line = buf+pos;
	for (p = line; p < end; p++) {
		if (*p == '\n')
			break;
	}
	if (p == end) {
		if (pos == 0) {
			/* buf is too small to hold this line */
			*linelenp = -3;
			return 0;
		}
		len = size - pos;
		memcpy(buf, line, len);
		gl->pos = 0;
		gl->len = len;
		return getline_line(gl, linelenp);
	}
	pos = (p - buf) + 1;
	len = p - line;
	line[len] = 0;
	gl->pos = pos;
	gl->len = len;

	*linelenp = len;

	return line;
}

getline_t *
getline_open (int fd)
{
	getline_t *gl;
	int size;

	size = 1024;
	gl = xmalloc(sizeof(getline_t) + size);
	gl->fd = fd;
	gl->size = size;
	gl->pos = 0;
	gl->len = 0;

	return gl;
}

void
getline_close (getline_t *gl)
{
	xfree(gl);
}
