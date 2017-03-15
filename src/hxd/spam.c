#include <string.h>
#include "hlserver.h"
#include "spam.h"

void
spam_update (struct htlc_conn *htlc, u_int32_t utype)
{
	conf_table_t *t = &hxd_cfg.spam.spamconf;
	unsigned int pos, size, nta = t->nta;
	struct tablearray *ta = t->ta;
	int type = (int)utype;

	if ((loopZ_timeval.tv_sec - htlc->spam_time) >= htlc->spam_time_limit) {
		htlc->spam_points = 0;
		htlc->spam_time = loopZ_timeval.tv_sec;
	}
	/* Find the entry for this type */
	pos = nta / 2;
	size = pos;
	while (size) {
		if (type == ta[pos].i[0])
			break;
		size = size%2==1 ? (size == 1 ? 0 : size / 2 + 1) : size / 2;
		if (type > ta[pos].i[0]) {
			pos += size;
		} else {
			pos -= size;
		}
	}
	if (!size) {
		pos = 0;
		if (ta[pos].i[0] != 0)
			return;
	}

	/* hxd_log("updating spam_points to %d { %d , %d }",htlc->spam_points,
			ta[pos].i[0], ta[pos].i[1]); */
	htlc->spam_points += ta[pos].i[1];
}

void
spam_sort (void)
{
	conf_table_t *t = &hxd_cfg.spam.spamconf;
	unsigned int i, j;
	int x[2];

	/* bubble sort */
	for (i = 0; i < t->nta; i++) {
		for (j = 1; j < t->nta - i; j++) {
			if (t->ta[j - 1].i[0] > t->ta[j].i[0]) {
				x[0] = t->ta[j - 1].i[0];
				x[1] = t->ta[j - 1].i[1];
				t->ta[j - 1].i[0] = t->ta[j].i[0];
				t->ta[j - 1].i[1] = t->ta[j].i[1];
				t->ta[j].i[0] = x[0];
				t->ta[j].i[1] = x[1];
			}
		}
	}
}

int
check_messagebot (const char *msg)
{
	unsigned int i;

	for (i = 0; hxd_cfg.spam.messagebot[i]; i++) {
		if (strstr(msg, hxd_cfg.spam.messagebot[i])) {
			return 1;
		}
	}

	return 0;
}

struct conntime {
	struct IN_ADDR addr;
	struct timeval tv;
};

#define MAX_NCT 128
static struct conntime ct[MAX_NCT];
static int nct = 0;

int
check_connections (struct SOCKADDR_IN *saddr)
{
	struct htlc_conn *htlcp;
	int i = 0;

	while (i < nct && tv_secdiff(&ct[i].tv, &loopZ_timeval) > hxd_cfg.spam.reconn_time)
		i++;
	if (i) {
		if (nct-i)
			memcpy(&ct[0], &ct[i], (nct-i)*sizeof(struct conntime));
		nct -= i;
	}
	/* could add a hash here */
	for (i = 0; i < nct; i++) {
		if (!memcmp(&saddr->SIN_ADDR, &ct[i].addr, sizeof(struct IN_ADDR)))
			return 2;
	}
	i = nct;
	if (i == MAX_NCT) {
		/* XXX slow */
		memcpy(&ct[0], &ct[1], (MAX_NCT-1)*sizeof(struct conntime));
		i--;
	}
	ct[i].addr = saddr->SIN_ADDR;
	ct[i].tv = loopZ_timeval;
	nct++;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!memcmp(&saddr->SIN_ADDR, &htlcp->sockaddr.SIN_ADDR, sizeof(saddr->SIN_ADDR))) {
			i++;
		}
	}
	if (i > hxd_cfg.spam.conn_max)
		return 1;

	return 0;
}
