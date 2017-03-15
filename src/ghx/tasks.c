#include "hx.h"
#include "hx_gtk.h"
#include "xmalloc.h"
#include "icons.h"

extern void task_tasks_update (struct htlc_conn *htlc);
extern void xfer_tasks_update (struct htlc_conn *htlc);

struct gtask {
	struct gtask *next, *prev;
	u_int32_t trans;
	struct htxf_conn *htxf;
	GtkWidget *label;
	GtkWidget *pbar;
	GtkWidget *listitem;
};

static struct gtask *
gtask_with_trans (struct ghtlc_conn *ghtlc, u_int32_t trans)
{
	struct gtask *gtsk;

	for (gtsk = ghtlc->gtask_list; gtsk; gtsk = gtsk->prev) {
		if (gtsk->trans == trans)
			return gtsk;
	}

	return 0;
}

static struct gtask *
gtask_with_htxf (struct ghtlc_conn *ghtlc, struct htxf_conn *htxf)
{
	struct gtask *gtsk;

	for (gtsk = ghtlc->gtask_list; gtsk; gtsk = gtsk->prev) {
		if (gtsk->htxf == htxf)
			return gtsk;
	}

	return 0;
}

static struct gtask *
gtask_new (struct ghtlc_conn *ghtlc, u_int32_t trans, struct htxf_conn *htxf)
{
	GtkWidget *pbar;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *listitem;
	struct gtask *gtsk;

	gtsk = xmalloc(sizeof(struct gtask));
	gtsk->next = 0;
	gtsk->prev = ghtlc->gtask_list;
	if (ghtlc->gtask_list)
		ghtlc->gtask_list->next = gtsk;

	pbar = gtk_progress_bar_new();
	label = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	vbox = gtk_vbox_new(0, 0);
	hbox = gtk_hbox_new(0, 0);
	gtk_widget_set_usize(vbox, 240, 40);
	gtk_box_pack_start(GTK_BOX(hbox), label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pbar, 1, 1, 0);

	listitem = gtk_list_item_new();
	gtk_object_set_data(GTK_OBJECT(listitem), "gtsk", gtsk);
	gtk_container_add(GTK_CONTAINER(listitem), vbox);
	if (ghtlc->gtask_gtklist) {
		GList *itemlist;

		itemlist = g_list_append(0, listitem);
		gtk_list_append_items(GTK_LIST(ghtlc->gtask_gtklist), itemlist);
		gtk_widget_show_all(listitem);
	}

	gtsk->label = label;
	gtsk->pbar = pbar;
	gtsk->listitem = listitem;
	gtsk->trans = trans;
	gtsk->htxf = htxf;
	ghtlc->gtask_list = gtsk;

	return gtsk;
}

static void
gtask_delete (struct ghtlc_conn *ghtlc, struct gtask *gtsk)
{
	if (ghtlc->gtask_gtklist) {
		GList *itemlist;

		itemlist = g_list_append(0, gtsk->listitem);
		gtk_list_remove_items(GTK_LIST(ghtlc->gtask_gtklist), itemlist);
		g_list_free(itemlist);
		/*gtk_widget_destroy(gtsk->listitem);*/
	}
	if (gtsk->next)
		gtsk->next->prev = gtsk->prev;
	if (gtsk->prev)
		gtsk->prev->next = gtsk->next;
	if (gtsk == ghtlc->gtask_list)
		ghtlc->gtask_list = gtsk->prev;
	xfree(gtsk);
}

static void
gtask_list_delete (struct ghtlc_conn *ghtlc)
{
	struct gtask *gtsk, *prev;
	GList *itemlist = 0;

	for (gtsk = ghtlc->gtask_list; gtsk; gtsk = prev) {
		prev = gtsk->prev;
		if (ghtlc->gtask_gtklist)
			itemlist = g_list_append(itemlist, gtsk->listitem);
		/*gtk_widget_destroy(gtsk->listitem);*/
		xfree(gtsk);
	}
	if (ghtlc->gtask_gtklist) {
		gtk_list_remove_items(GTK_LIST(ghtlc->gtask_gtklist), itemlist);
		g_list_free(itemlist);
		gtk_widget_destroy(ghtlc->gtask_gtklist);
		ghtlc->gtask_gtklist = 0;
	}
	ghtlc->gtask_list = 0;
}

void
task_update (struct htlc_conn *htlc, struct task *tsk)
{
	GtkWidget *pbar;
	GtkWidget *label;
	char taskstr[256];
	struct ghtlc_conn *ghtlc;
	struct gtask *gtsk;
	u_int32_t pos = tsk->pos, len = tsk->len;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gtsk = gtask_with_trans(ghtlc, tsk->trans);
	if (!gtsk)
		gtsk = gtask_new(ghtlc, tsk->trans, 0);
	label = gtsk->label;
	pbar = gtsk->pbar;
	snprintf(taskstr, sizeof(taskstr), "Task 0x%x (%s) %u/%u", tsk->trans, tsk->str ? tsk->str : "", pos, pos+len);
	gtk_label_set_text(GTK_LABEL(label), taskstr);
	if (pos)
		gtk_progress_bar_update(GTK_PROGRESS_BAR(pbar), (gfloat)pos / (gfloat)(pos + len));

	if (len == 0)
		gtask_delete(ghtlc, gtsk);
}

static void
tasks_destroy (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	gtask_list_delete(ghtlc);
}

static void
task_stop (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gtask *gtsk;
	GList *lp, *next;
	GtkWidget *listitem;

	if (!ghtlc->gtask_gtklist)
		return;
	for (lp = GTK_LIST(ghtlc->gtask_gtklist)->selection; lp; lp = next) {
		next = lp->next;
		listitem = (GtkWidget *)lp->data;
		gtsk = (struct gtask *)gtk_object_get_data(GTK_OBJECT(listitem), "gtsk");
		if (gtsk->htxf) {
			mutex_lock(&ghtlc->htlc->htxf_mutex);
			xfer_delete(gtsk->htxf);
			mutex_unlock(&ghtlc->htlc->htxf_mutex);
			gtask_delete(ghtlc, gtsk);
		}
	}
}

static void
task_go (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gtask *gtsk;
	GList *lp, *next;
	GtkWidget *listitem;

	if (!ghtlc->gtask_gtklist)
		return;
	for (lp = GTK_LIST(ghtlc->gtask_gtklist)->selection; lp; lp = next) {
		next = lp->next;
		listitem = (GtkWidget *)lp->data;
		gtsk = (struct gtask *)gtk_object_get_data(GTK_OBJECT(listitem), "gtsk");
		if (gtsk->htxf)
			xfer_go(gtsk->htxf);
	}
}

void
create_tasks_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *gtklist;
	GtkWidget *hbuttonbox;
	GtkWidget *topframe;
	GtkWidget *stopbtn;
	GtkWidget *gobtn;
	GtkTooltips *tooltips;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_TASKS))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_TASKS);
	window = gwin->widget;

	changetitle(ghtlc, window, "Tasks");

	topframe = gtk_frame_new(0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);

	tooltips = gtk_tooltips_new();
	stopbtn = icon_button_new(ICON_KICK, "Stop", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(stopbtn), "clicked",
				  GTK_SIGNAL_FUNC(task_stop), (gpointer)ghtlc);
	gobtn = icon_button_new(ICON_GO, "Go", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(stopbtn), "clicked",
				  GTK_SIGNAL_FUNC(task_go), (gpointer)ghtlc);

	hbuttonbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), stopbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), gobtn, 0, 0, 2);
	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox);
	vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), topframe, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtklist = gtk_list_new();
	gtk_list_set_selection_mode(GTK_LIST(gtklist), GTK_SELECTION_EXTENDED);
	scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(scroll) = 0;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), gtklist);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, 1, 1, 0);

	gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				  GTK_SIGNAL_FUNC(tasks_destroy), (gpointer)ghtlc);

	gtask_list_delete(ghtlc);
	ghtlc->gtask_gtklist = gtklist;

	task_tasks_update(ghtlc->htlc);
	xfer_tasks_update(ghtlc->htlc);

	gtk_widget_show_all(window);
}

void
open_tasks (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_tasks_window(ghtlc);
}

extern void human_time (time_t ts, char *buf);

void
file_update (struct htxf_conn *htxf)
{
	GtkWidget *pbar;
	GtkWidget *label;
	struct ghtlc_conn *ghtlc;
	struct gtask *gtsk;
	char str[4096];
	char humanbuf[LONGEST_HUMAN_READABLE*3+3], *posstr, *sizestr, *bpsstr;
	u_int32_t pos, size;
	struct timeval now;
	time_t sdiff, usdiff, Bps, eta;
	char etastr[32];

	ghtlc = ghtlc_conn_with_htlc(htxf->htlc);
	if (!ghtlc)
		return;
	gtsk = gtask_with_htxf(ghtlc, htxf);
	if (!gtsk)
		gtsk = gtask_new(ghtlc, 0, htxf);
	label = gtsk->label;
	pbar = gtsk->pbar;
	pos = htxf->total_pos;
	size = htxf->total_size;

	gettimeofday(&now, 0);
	sdiff = now.tv_sec - htxf->start.tv_sec;
	usdiff = now.tv_usec - htxf->start.tv_usec;
	if (!sdiff)
		sdiff = 1;
	Bps = pos / sdiff;
	if (!Bps)	
		Bps = 1;
	eta = (size - pos) / Bps
	    + ((size - pos) % Bps) / Bps;
	human_time(eta, etastr);

	posstr = human_size(pos, humanbuf);
	sizestr = human_size(size, humanbuf+LONGEST_HUMAN_READABLE+1);
	bpsstr = human_size(Bps, humanbuf+LONGEST_HUMAN_READABLE*2+2);
	snprintf(str, sizeof(str), "%s  %s/%s  %s/s  ETA: %s  %s",
		 htxf->type & XFER_GET ? "get" : "put",
		 posstr, sizestr, bpsstr, etastr, htxf->path);
	gtk_label_set_text(GTK_LABEL(label), str);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(pbar),
				size ? (gfloat)pos / (float)size : 1.0);

	if (pos == size)
		gtask_delete(ghtlc, gtsk);
}
