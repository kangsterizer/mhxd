#ifndef __treg_h
#define __treg_h

#include <sys/types.h>

#define TSPOOF 1

#ifdef TSPOOF
#include <netinet/ip.h>
#include <netinet/udp.h>
#endif

struct server {
	char *name;
	char *desc;
	struct in_addr addr;
	struct in_addr faddr;
	struct in_addr taddr;
	u_int32_t id;
	u_int16_t port;
	u_int16_t nusers;
	u_int16_t nlen;
	u_int16_t dlen;
};
typedef struct server server_t;

struct htrk_pkt {
#ifdef TSPOOF
	struct iphdr ip;
	struct udphdr udp;
#endif
	u_int16_t version;
	u_int16_t port;
	u_int16_t nusers;
	u_int16_t __reserved0;
	u_int32_t id;
};

extern void treg_add (struct in_addr *daddr, struct in_addr *saddr, struct in_addr *fakeaddr, u_int16_t port, u_int32_t id, const char *name, const char *desc, u_int16_t nusers);
extern void treg_loop (void);

#endif
