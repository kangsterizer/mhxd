#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include "hlserver.h"
#include "xmalloc.h"
#include "trx.h"

#ifdef SOCKS
#include "socks.h"
#endif

struct tracker {
	struct SOCKADDR_IN sockaddr;
	u_int32_t id;
	char plen, password[31];
};

static int ntrackersocks = 0;
static int *tracker_udpsocks = 0;

static int ntrackers = 0;
static struct tracker *trackers = 0;

static u_int8_t tracker_buf[sizeof(struct htrk_hdr) + 1024];
static u_int16_t tracker_buf_len = 0;

int
return_num_users (void)
{
	struct htlc_conn *htlcp;
	int i = 0;
	
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->access_extra.can_login)
			continue;
		i++;
	}
	
	return i;
}

int
tracker_register_timer (void *__arg)
{
	struct htrk_hdr *pkt = (struct htrk_hdr *)tracker_buf;
	int i, j, len, plen;

	pkt->port = htons(hxd_cfg.options.port);
	if (hxd_cfg.tracker.nusers >= 0)
		pkt->nusers = htons(hxd_cfg.tracker.nusers);
	else
		pkt->nusers = htons(return_num_users());

	for (i = 0; i < ntrackers; i++) {
		pkt->id = htonl(trackers[i].id);
		len = tracker_buf_len;
		plen = trackers[i].plen;
		tracker_buf[len++] = plen;
		if (plen) {
			memcpy(&tracker_buf[len], trackers[i].password, plen);
			len += plen;
		}
		for (j = 0; j < ntrackersocks; j++)
			(void)sendto(tracker_udpsocks[j], tracker_buf, len, 0,
				     (struct sockaddr *)&trackers[i].sockaddr, sizeof(struct SOCKADDR_IN));
	}
	if (__arg)
		return 1;
	else
		return 0;
}

void
tracker_register_init (void)
{
	struct htrk_hdr *pkt = (struct htrk_hdr *)tracker_buf;
	struct SOCKADDR_IN saddr;
	struct IN_ADDR inaddr;
	u_int16_t len;
	u_int8_t nlen, dlen;
	int i;
	u_int32_t id;
	char *trp, *pass, *host;

	ntrackers = 0;
	if (!hxd_cfg.tracker.trackers) {
		if (trackers) {
			xfree(trackers);
			trackers = 0;
		}
		return;
	}
	for (i = 0; (trp = hxd_cfg.tracker.trackers[i]); i++) {
		host = strrchr(trp, '@');
		if (!host) {
			host = trp;
			id = 0;
			pass = 0;
		} else {
			*host = 0;
			host++;
			pass = strchr(trp, ':');
			if (pass)
				*pass = 0;
			id = strtoul(trp, 0, 0);
			if (pass) {
				*pass = 0;
				pass++;
			}
		}
#ifdef CONFIG_IPV6
		if (!inet_pton(AFINET, host, &inaddr)) {
#else
		if (!inet_aton(host, &inaddr)) {
#endif
			struct hostent *he = gethostbyname(host);
			if (he)
				memcpy(&inaddr, he->h_addr_list[0],
				       (unsigned)he->h_length > sizeof(inaddr) ?
				       sizeof(inaddr) : (unsigned)he->h_length);
			else {
				hxd_log("could not resolve tracker hostname %s", host);
				/* exit(1); */
				continue;
			}
		}
		trackers = xrealloc(trackers, sizeof(struct tracker)*(ntrackers+1));
		trackers[ntrackers].sockaddr.SIN_ADDR = inaddr;
		trackers[ntrackers].sockaddr.SIN_PORT = htons(HTRK_UDPPORT);
		trackers[ntrackers].sockaddr.SIN_FAMILY = AFINET;
		trackers[ntrackers].id = id;
		if (pass) {
			int plen = strlen(pass);
			if (plen > 30)
				plen = 30;
			memcpy(trackers[ntrackers].password, pass, plen);
			trackers[ntrackers].password[plen] = 0;
			trackers[ntrackers].plen = plen;
		} else {
			trackers[ntrackers].plen = 0;
			trackers[ntrackers].password[0] = 0;
		}
		ntrackers++;
	}
	if (!ntrackers)
		return;

	for (i = 0; i < ntrackersocks; i++)
		close(tracker_udpsocks[i]);
	ntrackersocks = 0;

	for (i = 0; (host = hxd_cfg.options.addresses[i]); i++) {
#ifdef CONFIG_IPV6
		if (!inet_pton(AFINET, host, &saddr.SIN_ADDR)) {
#else
		if (!inet_aton(host, &saddr.SIN_ADDR)) {
#endif
			struct hostent *he = gethostbyname(host);
			if (he) {
				memcpy(&saddr.SIN_ADDR, he->h_addr_list[0],
				       (size_t)he->h_length > sizeof(saddr.SIN_ADDR) ?
				       sizeof(saddr.SIN_ADDR) : (size_t)he->h_length);
			} else {
				hxd_log("%s:%d: could not resolve hostname %s", __FILE__, __LINE__, host);
				/* exit(1); */
				continue;
			}
		}

		tracker_udpsocks = xrealloc(tracker_udpsocks, sizeof(int)*(ntrackersocks+1));
		tracker_udpsocks[ntrackersocks] = socket(AFINET, SOCK_DGRAM, IPPROTO_UDP);
		if (tracker_udpsocks[ntrackersocks] < 0) {
			hxd_log("tracker_init: socket: %s", strerror(errno));
			/* exit(1); */
			continue;
		}
		saddr.SIN_FAMILY = AFINET;
		saddr.SIN_PORT = 0; /* htons(32769); */
		if (bind(tracker_udpsocks[ntrackersocks], (struct sockaddr *)&saddr, sizeof(saddr))) {
			hxd_log("tracker_init: bind: %s", strerror(errno));
			close(tracker_udpsocks[ntrackersocks]);
			/* exit(1); */
			continue;
		}
		fd_closeonexec(tracker_udpsocks[ntrackersocks], 1);
		ntrackersocks++;
	}
	if (!ntrackersocks)
		return;

	len = sizeof(struct htrk_hdr);
	nlen = strlen(hxd_cfg.tracker.name);
	tracker_buf[len++] = nlen;
	memcpy(&tracker_buf[len], hxd_cfg.tracker.name, nlen);
	len += nlen;
	dlen = strlen(hxd_cfg.tracker.description);
	tracker_buf[len++] = dlen;
	memcpy(&tracker_buf[len], hxd_cfg.tracker.description, dlen);
	len += dlen;
	tracker_buf_len = len;
	pkt->version = htons(0x0001);
	pkt->__reserved0 = 0;
	pkt->id = 0;

	timer_add_secs(hxd_cfg.tracker.interval, tracker_register_timer, trackers);
}
