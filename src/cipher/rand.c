#include "hxd.h"
#include "cipher.h"
#include <unistd.h>
#include <fcntl.h>

#if USE_OPENSSL
#include <openssl/rand.h>
/* zlib has a conflicting typedef with err.h */
/* #include <openssl/err.h> */
extern unsigned long ERR_get_error (void);
extern char *ERR_error_string (unsigned long e, char *buf);
#endif

unsigned int
random_bytes (u_int8_t *buf, unsigned int nbytes)
{
#if !USE_OPENSSL
	int fd;
#endif
	int ok;

#if USE_OPENSSL
#if USE_OLD_OPENSSL
	unsigned int i;

	memset(buf, 0, nbytes);
	RAND_bytes(buf, nbytes);
	ok = 0;
	for (i = 0; i < nbytes; i++) {
		if (buf[i] != 0) {
			ok = 1;
			break;
		}
	}
#else
	if (!RAND_bytes(buf, nbytes)) {
		char buf[120];
		ERR_error_string(ERR_get_error(), buf);
		hxd_log("RAND_bytes failed: %s", buf);
		ok = 0;
	} else {
		ok = 1;
	}
#endif /* old openssl */
#else
	fd = SYS_open("/dev/urandom", O_RDONLY, 0);
	if (fd >= 0) {
		if (read(fd, buf, nbytes) != nbytes)
			ok = 0;
		else
			ok = 1;
		close(fd);
	} else {
		ok = 0;
	}
#endif

	if (ok)
		return nbytes;
	else
		return 0;
}
