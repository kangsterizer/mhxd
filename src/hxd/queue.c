#include "hlserver.h"
#include "xmalloc.h"
#include "queue.h"

extern u_int16_t nr_queued;

static int refresh_in_progress = 0;
static hxd_mutex_t queue_mutex;

static int
queue_compare (const void *p1, const void *p2)
{
	struct htxf_conn *htxf1 = *(struct htxf_conn **)p1, *htxf2 = *(struct htxf_conn **)p2;

	if (htxf1->queue_pos > htxf2->queue_pos)
		return 1;
	else if (htxf1->queue_pos != htxf2->queue_pos)
		return -1;
	else
		return 0;
}

int
queue_refresher (void *unused __attribute__((__unused__)))
{
	struct htlc_conn *htlcp;
	struct htxf_conn **htxfs;
	u_int32_t htxfs_count, num_shift;
	unsigned int free_slots;
	u_int32_t active_dls;
	u_int32_t atg, i;
	u_int16_t queue_pos;

	atg = nr_gets;

	htxfs = xmalloc((atg+1)*sizeof(struct htxf_conn *));

	htxfs[0] = 0;
	active_dls = 0;
	htxfs_count = 0;
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		mutex_lock(&htlcp->htxf_mutex);
		for (i = 0; i < HTXF_GET_MAX; i++) {
			if (htlcp->htxf_out[i]) {
				if (htlcp->htxf_out[i]->gone)
					continue;
				if (htlcp->htxf_out[i]->queue_pos == 0)
					active_dls++;
				htxfs[htxfs_count] = htlcp->htxf_out[i];
				htxfs_count++;
				htxfs[htxfs_count] = 0;
			}
		}
	}
	free_slots = hxd_cfg.limits.total_downloads - active_dls;

	/* hxd_log("Entering queue_refresher(): total_dls:%d active_dls:%d free_slots:%d", atg, active_dls, free_slots); */

	qsort(htxfs, htxfs_count, sizeof(struct htxf_conn *), queue_compare);

	num_shift = 0;
	while (free_slots > 0) {
		if (htxfs[num_shift] == 0)
			break;

		htxfs[num_shift]->queue_pos = 0;
		hxd_log("Queue Change: Download started for %x",
			htxfs[num_shift]->ref);
		queue_pos = 0;
		hlwrite(htxfs[num_shift]->htlc, HTLS_HDR_QUEUE_UPDATE, 0, 2,
			HTLS_DATA_HTXF_REF, sizeof(htxfs[i]->ref), &htxfs[num_shift]->ref,
			HTLS_DATA_QUEUE_POSITION, sizeof(queue_pos), &queue_pos);

		nr_queued--;
		free_slots--;
		num_shift++;
	}

	nr_queued = 0;
	if (num_shift) {
		for (i = num_shift; htxfs[i]; i++) {
			queue_pos = i - num_shift + 1;
			hxd_log("Changing Queue: Pos %d, New %d for %s",
				htxfs[i]->queue_pos, queue_pos, htxfs[i]->htlc->name);
			htxfs[i]->queue_pos = queue_pos;
			queue_pos = htons(queue_pos);
			hlwrite(htxfs[i]->htlc, HTLS_HDR_QUEUE_UPDATE, 0, 2,
				HTLS_DATA_HTXF_REF, sizeof(htxfs[i]->ref), &htxfs[i]->ref,
				HTLS_DATA_QUEUE_POSITION, sizeof(queue_pos), &queue_pos);
			nr_queued++;
		}
	}

	/* hxd_log("Exiting queue_refresher(): nr_queued:%d", nr_queued); */
	xfree(htxfs);

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		mutex_unlock(&htlcp->htxf_mutex);
	}

	refresh_in_progress = 0;

	return 0;
}

void
queue_refresh (void)
{
	mutex_lock(&queue_mutex);

	if (refresh_in_progress)
		timer_delete_ptr((void *)queue_refresh);
	timer_add_secs(1, queue_refresher, (void *)queue_refresh);
	refresh_in_progress = 1;

	mutex_unlock(&queue_mutex);
}

void
queue_init (void)
{
	mutex_init(&queue_mutex);
}
