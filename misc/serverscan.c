/* look for hotline servers on an 8 bit subnet */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
fd_blocking (int fd, int on)
{
	int x;

#if defined(_POSIX_SOURCE) || !defined(FIONBIO)
#if !defined(O_NONBLOCK)
# if defined(O_NDELAY)
#  define O_NONBLOCK O_NDELAY
# endif
#endif
	if ((x = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
	if (on)
		x &= ~O_NONBLOCK;
	else
		x |= O_NONBLOCK;

	return fcntl(fd, F_SETFL, x);
#else
	x = on;

	return ioctl(fd, FIONBIO, &x);
#endif
}

int
main (int argc, char **argv)
{
	struct sockaddr_in saddr;
	fd_set writefds, wfds;
	int n, s, high_s;
	unsigned int addr, i;

	inet_aton(argv[1] ? argv[1] : "127.0.0.0", &saddr.sin_addr);
	addr = saddr.sin_addr.s_addr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(5500);
	for (i = 1; i < 255; i++) {
		saddr.sin_addr.s_addr = htonl(ntohl(addr) | i);
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0) {
			perror("socket");
			exit(1);
		}
		fd_blocking(s, 0);
		if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr))) {
			if (errno != EINPROGRESS) {
				perror("connect");
				exit(1);
			}
		}
		FD_SET(s, &writefds);
	}
	high_s = s;
	for (;;) {
		wfds = writefds;
		n = select(high_s + 1, 0, &wfds, 0, 0);
		if (n < 0) {
			perror("select");
			exit(1);
		}
		for (s = 3; n && s <= high_s; s++) {
			if (FD_ISSET(s, &wfds)) {
				int siz = sizeof(saddr);
				if (!getpeername(s, &saddr, &siz)) {
					unsigned char magic[8];
					fd_blocking(s, 1);
					write(s, "TRTPHOTL\0\1\0\2", 12);
					if (read(s, magic, 8) == 8 && !memcmp(magic, "TRTP\0\0\0\0", 8)) {
						printf("%s\n", inet_ntoa(saddr.sin_addr));
						close(s);
						exit(0);
					}
				}
				close(s);
				FD_CLR(s, &writefds);
				n--;
			}
		}
	}

	return 0;
}
