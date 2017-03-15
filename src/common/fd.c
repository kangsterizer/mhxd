#if !defined(__WIN32__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
	x = !on;

	return ioctl(fd, FIONBIO, &x);
#endif
}
#else
#include <windows.h>
#include <winsock.h>
#endif

int
socket_blocking (int s, int on)
{
#if defined(__WIN32__)
	int x;
	x = !on;
	if (ioctlsocket(s, FIONBIO, (unsigned long *)&x))
		return -1;
	return 0;
#else
	return fd_blocking(s, on);
#endif
}

int
fd_closeonexec (int fd, int on)
{
#if defined(__WIN32__)
	return -1;
#else
	int x;

	if ((x = fcntl(fd, F_GETFD, 0)) == -1)
		return -1;
	if (on)
		x &= ~FD_CLOEXEC;
	else
		x |= FD_CLOEXEC;

	return fcntl(fd, F_SETFD, x);
#endif
}

int
fd_lock_write (int fd)
{
#if defined(__WIN32__)
	return -1;
#else
	struct flock lk;

	lk.l_type = F_WRLCK;
	lk.l_start = 0;
	lk.l_whence = SEEK_SET;
	lk.l_len = 0;
	lk.l_pid = getpid();

	return fcntl(fd, F_SETLK, &lk);
#endif
}
