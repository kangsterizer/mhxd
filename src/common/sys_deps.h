#ifndef __hxd_sys_deps_h
#define __hxd_sys_deps_h

#if defined(__WIN32__)
#define SYS_open(x,y,z) open(x,y|O_BINARY,z)
#define SYS_mkdir(x,y) mkdir(x)
#define SYS_fsync(x)
#define SYS_lstat(x,y) stat(x,y)
#define SYS_symlink(x,y) -1
#define SETSOCKOPT_PTR_CAST_T char *
#define S_ISLNK(x) 0
#include <time.h>
#include <winsock.h>
static __inline__ void
gettimeofday (struct timeval *tv, void *xxx __attribute__((__unused__)))
{
	time_t t = time(0);
	tv->tv_sec = t;
	tv->tv_usec = 0;
}
#define socket_close(x) closesocket(x)
#define socket_read(a,b,c) recv(a,b,c,0)
#define socket_write(a,b,c) send(a,b,c,0)
#define socket_errno() (WSAGetLastError())
#else
#define SYS_open(x,y,z) open(x,y,z)
#define SYS_mkdir(x,y) mkdir(x,y)
#define SYS_fsync(x) fsync(x)
#define SYS_lstat(x,y) lstat(x,y)
#define SYS_symlink(x,y) symlink(x,y)
#define SETSOCKOPT_PTR_CAST_T int *
#define socket_close(x) close(x)
#define socket_read(a,b,c) read(a,b,c)
#define socket_write(a,b,c) write(a,b,c)
#define socket_errno() errno
#endif

#endif
