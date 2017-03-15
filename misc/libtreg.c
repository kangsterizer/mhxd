#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libtreg.h"

static void die (const char *msg) { perror(msg); exit(1); }

static server_t *servers;
static u_int nservers = 0;
static u_int servers_size = 0;

void
server_reg (server_t *s, int usock, struct sockaddr_in *saddr, u_int8_t *buf)
{
	struct htrk_pkt *pkt = (struct htrk_pkt *)buf;
	int len;

	pkt->ip.saddr = s->addr.s_addr;
	pkt->ip.daddr = s->taddr.s_addr;
	pkt->port = s->port;
	pkt->id = s->id;
	pkt->nusers = s->nusers;
	len = sizeof(struct htrk_pkt);
	buf[len++] = s->nlen;
	memcpy(&buf[len], s->name, s->nlen);
	len += s->nlen;
	buf[len++] = s->dlen;
	memcpy(&buf[len], s->desc, s->dlen);
	len += s->dlen;
	buf[len] = 0;
	len++;
	pkt->ip.tot_len = htons(len);
	pkt->udp.len = htons(len - sizeof(struct iphdr));
	saddr->sin_addr = s->taddr;
	if (sendto(usock, buf, len, 0, (struct sockaddr *)saddr, sizeof(struct sockaddr_in)) < 0)
		perror("sendto");
}

void
treg_loop (void)
{
	struct sockaddr_in saddr;
	int usock;
	unsigned char buf[sizeof(struct htrk_pkt) + 256 + 256 + 4];
	struct htrk_pkt *pkt = (struct htrk_pkt *)buf;
	int sopt = 1;
	u_int i;
	server_t *s;

#ifdef TSPOOF
	if ((usock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		die("raw socket");
	if (setsockopt(usock, IPPROTO_IP, IP_HDRINCL, &sopt, sizeof(sopt)))
		die("IP_HDRINCL");
#else
	if ((usock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		die("socket");
#endif
	saddr.sin_family = AF_INET;
	saddr.sin_port = 0; /* htons(32769); */
	saddr.sin_addr.s_addr = 0;
	if (bind(usock, (struct sockaddr *)&saddr, sizeof(saddr)))
		die("bind");

	memset(buf, 0, sizeof(buf));
	pkt->version = htons(0x0001);
	//pkt->port = htons(5500);
	//pkt->nusers = htons(69);
	pkt->__reserved0 = 0;
	//pkt->id = htonl(0xefe44d00); // ?? b20 ?
	//pkt->id = htonl(0xa67985b1); // 1.2.3
	//pkt->id = htonl(0x486d1f7c); // AfterBirth a2c3
	//pkt->id = htonl(0x7f23a207); // 1.8.4
	// random field
	//pkt->id = htonl(0);

#ifdef TSPOOF
	pkt->ip.version = 4;
	pkt->ip.ihl = 5;
	pkt->ip.tos = 0;
	//pkt->ip.tot_len = htons(len);
	pkt->ip.id = htons(0x7a69);
	pkt->ip.frag_off = htons(0x4000);
	pkt->ip.ttl = 255;
	pkt->ip.protocol = IPPROTO_UDP;
	pkt->ip.check = 0;
	//pkt->ip.saddr = 0;
	//pkt->ip.daddr = 0;
	pkt->udp.source = htons(32769);
	pkt->udp.dest = htons(5499);
	//pkt->udp.len = htons(len - sizeof(struct iphdr));
	pkt->udp.check = 0;
#endif
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(5499);

	for (i = 0; i < nservers; i++) {
		s = &servers[i];
		if (s->addr.s_addr == s->faddr.s_addr)
			continue;
		s->id = 0;
		if (s->nlen)
			s->name[0] += 1;
		server_reg(s, usock, &saddr, buf);
		s->addr = s->faddr;
		server_reg(s, usock, &saddr, buf);
		s->name[0] -= 1;
		server_reg(s, usock, &saddr, buf);
	}
	for (;;) {
		for (i = 0; i < nservers; i++) {
			s = &servers[i];
			server_reg(s, usock, &saddr, buf);
		}
		sleep(300);
	}
}

void
treg_add (struct in_addr *daddr, struct in_addr *saddr, struct in_addr *fakeaddr, u_int16_t port, u_int32_t id, const char *name, const char *desc, u_int16_t nusers)
{
	server_t *s;

	if (nservers >= servers_size) {
		servers = realloc(servers, (servers_size+64)*sizeof(server_t));
		if (!servers)
			die("realloc");
		servers_size += 64;
	}
	s = &servers[nservers];
	nservers++;
	s->addr = *saddr;
	s->faddr = *fakeaddr;
	s->taddr = *daddr;
	s->port = htons(port);
	s->name = strdup(name);
	if (!s->name)
		die("strdup");
	s->desc = strdup(desc);
	if (!s->desc)
		die("strdup");
	s->nlen = strlen(name);
	s->dlen = strlen(desc);
	s->nusers = htons(nusers);
	s->id = htonl(id);
}
