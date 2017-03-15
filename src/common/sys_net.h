#ifndef __hxd_sys_net_h
#define __hxd_sys_net_h

#if defined(__WIN32__)
#define __USE_W32_SOCKETS
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#endif
