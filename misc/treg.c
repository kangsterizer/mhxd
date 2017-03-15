#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if !defined(__GNUC__) || defined(__STRICT_ANSI__)
#define __attribute__(x)
#endif

#ifdef TSPOOF
#include <netinet/ip.h>
#include <netinet/udp.h>

#define SOURCE		"127.0.0.1"
#endif

#define NAME		"HoT WaReZ"
#define DESCRIPTION	"GiLLiAn AnDeRs0n NaEk3d!!"
#define TRACKER		"hltracker.com"

static char name[256] = NAME, namelen = sizeof(NAME) - 1,
	    description[256] = DESCRIPTION, descriptionlen = sizeof(DESCRIPTION) - 1;

struct htrk_pkt {
#ifdef TSPOOF
	struct iphdr ip;
	struct udphdr udp;
#endif
	unsigned short version;
	unsigned short port;
	unsigned short nusers;
	unsigned short __reserved0;
	unsigned int id;
};

static void die (char *msg) { perror(msg); exit(1); }

void traxor_loop (struct in_addr *, struct in_addr *) __attribute__((__noreturn__));

void
traxor_loop (struct in_addr *dst_addr, struct in_addr *src_addr)
{
	struct sockaddr_in saddr;
	int usock;
	unsigned short len;
	unsigned char buf[sizeof(struct htrk_pkt) + sizeof(name) + sizeof(description) + 3];
	struct htrk_pkt pkt;

#ifdef TSPOOF
	if ((usock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		die("raw socket");
        {
		int x = 1;
		if (setsockopt(usock, IPPROTO_IP, IP_HDRINCL, &x, sizeof(x)))
			die("IP_HDRINCL");
	}
#else
	if ((usock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		die("socket");
#endif
	saddr.sin_family = AF_INET;
	saddr.sin_port = 0; /* htons(32769); */
#ifdef TSPOOF
	saddr.sin_addr.s_addr = 0;
#else
	saddr.sin_addr = *src_addr;
#endif
	if (bind(usock, (struct sockaddr *)&saddr, sizeof(saddr)))
		die("bind");

	pkt.version = htons(0x0001);
	pkt.port = htons(5500);
	pkt.nusers = htons(69);
	pkt.__reserved0 = 0;
	//pkt.id = htonl(0xefe44d00); // ?? b20 ?
	//pkt.id = htonl(0xa67985b1); // 1.2.3
	//pkt.id = htonl(0x486d1f7c); // AfterBirth a2c3
	//pkt.id = htonl(0x7f23a207); // 1.8.4
	// random field
	pkt.id = htonl(0);

	len = sizeof(struct htrk_pkt);
	buf[len++] = namelen;
	memcpy(&buf[len], name, namelen);
	len += namelen;
	buf[len++] = descriptionlen;
	memcpy(&buf[len], description, descriptionlen);
	len += descriptionlen;
	buf[len] = 0;
	len++;

#ifdef TSPOOF
	pkt.ip.version = 4;
	pkt.ip.ihl = 5;
	pkt.ip.tos = 0;
	pkt.ip.tot_len = htons(len);
	pkt.ip.id = htons(0x7a69);
	pkt.ip.frag_off = htons(0x4000);
	pkt.ip.ttl = 255;
	pkt.ip.protocol = IPPROTO_UDP;
	pkt.ip.check = 0;
	pkt.ip.saddr = src_addr->s_addr;
	pkt.ip.daddr = dst_addr->s_addr;
	pkt.udp.source = htons(32769);
	pkt.udp.dest = htons(5499);
	pkt.udp.len = htons(len - sizeof(struct iphdr));
	pkt.udp.check = 0;
#endif
	memcpy(buf, &pkt, sizeof(struct htrk_pkt));

	saddr.sin_family = AF_INET;
	saddr.sin_addr = *dst_addr;
	saddr.sin_port = htons(5499);

	for (;;) {
		if (sendto(usock, buf, len, 0, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
			perror("sendto");
		sleep(300);
	}
}

int
main (int argc, char **argv)
{
	char *dst_host, *src_host = 0;
	struct in_addr dst_addr, src_addr;

#ifdef TSPOOF
	src_host = SOURCE;
#endif

	if (argc > 1) {
		dst_host = argv[1];
		if (argc > 2) {
			strncpy(name, argv[2], sizeof(name) - 1);
			namelen = strlen(name);
		}
		if (argc > 3) {
			strncpy(description, argv[3], sizeof(description) - 1);
			descriptionlen = strlen(description);
		}
		if (argc > 4)
			src_host = argv[4];
	} else {
		dst_host = TRACKER;
	}

	if (!inet_aton(dst_host, &dst_addr)) {
		struct hostent *he = gethostbyname(dst_host);
		if (he)
			memcpy(&dst_addr, he->h_addr_list[0], he->h_length > sizeof(dst_addr) ? sizeof(dst_addr) : he->h_length);
		else
			die(dst_host);
	}
	if (src_host) {
		if (!inet_aton(src_host, &src_addr)) {
			struct hostent *he = gethostbyname(src_host);
			if (he)
				memcpy(&src_addr, he->h_addr_list[0], he->h_length > sizeof(src_addr) ? sizeof(src_addr) : he->h_length);
			else
				die(src_host);
		}
	} else {
		memset(&src_addr, 0, sizeof(src_addr));
	}

	traxor_loop(&dst_addr, &src_addr);

	return 0;
}
