#ifndef __spam_h
#define __spam_h

#include "hxd.h"

extern void spam_update (struct htlc_conn *htlc, u_int32_t utype);
extern void spam_sort (void);
extern int check_messagebot (const char *msg);
extern int check_connections (struct SOCKADDR_IN *saddr);

#endif
