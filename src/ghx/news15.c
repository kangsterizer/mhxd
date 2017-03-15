/*
 * Copyright (C) 2000-2001 Misha Nasledov <misha@nasledov.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* taken from gtkhx and modified for hxd/ghx */

#include "hx.h"
#include "hx_gtk.h"
#include "hxd.h"
#include "news.h"
#include "gtk_hlist.h"
#include "xmalloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <sys/time.h>
#include <time.h>
#include "sys_net.h"

#include "newscat.xpm"
#include "newsfld.xpm"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#ifndef _
#define _(_x) (_x)
#endif

static struct gnews_folder *gfnews_with_hlist (struct ghtlc_conn *ghtlc, GtkWidget *hlist);

static void
newsf_clicked (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct ghtlc_conn *ghtlc;
	struct gnews_folder *gfnews;
	struct folder_item *item;
	char path[4096];
	int row, col;

	ghtlc = (struct ghtlc_conn *)data;
	gfnews = gfnews_with_hlist(ghtlc, widget);
	if (!gfnews)
		return;

	gtk_hlist_get_selection_info(GTK_HLIST(widget), event->x, event->y, &row, &col);
	if (event->type == GDK_2BUTTON_PRESS) {
		item = gtk_hlist_get_row_data(GTK_HLIST(widget), gfnews->row);
		if (item) {
			if (gfnews->news) {
				snprintf(path, sizeof(path), "%s/%s",
						gfnews->news->path ? gfnews->news->path : "",
						item->name);
				if (item->type == 1 || item->type == 2)
					hx_tnews_list(ghtlc->htlc, path, 0);
				else
					hx_tnews_list(ghtlc->htlc, path, 1);
			}
		}
	} else {
		gfnews->row = row;
		gfnews->col = col;
	}
}

static struct gnews_folder *
gfnews_with_hlist (struct ghtlc_conn *ghtlc, GtkWidget *hlist)
{
	struct gnews_folder *gfnews;

	for (gfnews = ghtlc->gfnews_list; gfnews; gfnews = gfnews->prev) {
		if (gfnews->news_list == hlist) {
			return gfnews;
		}
	}

	return 0;
}

static struct gnews_folder *
gfnews_with_path (struct ghtlc_conn *ghtlc, char *path)
{
	struct gnews_folder *gfnews;

	for (gfnews = ghtlc->gfnews_list; gfnews; gfnews = gfnews->prev) {
		if (!gfnews->news)
			continue;
		if (!strcmp(gfnews->news->path, path)) {
			return gfnews;
		}
	}

	return 0;
}

static void
delete_gfnews (struct gnews_folder *gfnews)
{
	struct ghtlc_conn *ghtlc = gfnews->ghtlc;

	if (gfnews->next)
		gfnews->next->prev = gfnews->prev;
	if (gfnews->prev)
		gfnews->prev->next = gfnews->next;
	if (gfnews == ghtlc->gfnews_list)
		ghtlc->gfnews_list = gfnews->prev;
	xfree(gfnews);
}

static void
destroy_gfnews_browser (GtkWidget *widget, gpointer data)
{
	struct gnews_folder *gfnews = (struct gnews_folder *)data;

	delete_gfnews(gfnews);
}

struct gnews_folder *
create_gfnews_window (struct ghtlc_conn *ghtlc, struct news_folder *news)
{
	struct gnews_folder *gfnews = xmalloc(sizeof(struct gnews_folder));
	GtkWidget *news_window;
	GtkWidget *news_list;
	GtkWidget *news_scroll;
	GtkWidget *topframe;
	GtkWidget *hbuttonbox;
	GtkWidget *vbox;
	GtkStyle *style;

	gfnews = xmalloc(sizeof(struct gnews_folder));
	memset(gfnews, 0, sizeof(struct gnews_folder));
	gfnews->ghtlc = ghtlc;
	gfnews->next = 0;
	gfnews->news = news;

	gfnews->prev = ghtlc->gfnews_list;
	if (ghtlc->gfnews_list)
		ghtlc->gfnews_list->next = gfnews;
	ghtlc->gfnews_list = gfnews;

	news_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(news_window), 1, 1, 0);

	gtk_widget_realize(news_window);
	style = gtk_widget_get_style(news_window);
	gtk_widget_set_usize(news_window, 264, 400);
	gtk_window_set_title(GTK_WINDOW(news_window), news->path);
	gtk_object_set_data(GTK_OBJECT(news_window), "gfnews", gfnews);
	gtk_signal_connect(GTK_OBJECT(news_window), "destroy", 
			   GTK_SIGNAL_FUNC(destroy_gfnews_browser), gfnews);

	news_scroll = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(news_scroll), 
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	news_list = gtk_hlist_new(1);
	gtk_hlist_set_column_width(GTK_HLIST(news_list), 0, 64);
/*	gtk_hlist_set_column_width(GTK_HLIST(news_list), 1, 240); */
	gtk_hlist_set_row_height(GTK_HLIST(news_list), 18);
	gtk_hlist_set_shadow_type(GTK_HLIST(news_list), GTK_SHADOW_NONE);
	gtk_hlist_set_column_justification(GTK_HLIST(news_list), 0, GTK_JUSTIFY_LEFT);
	gtk_signal_connect(GTK_OBJECT(news_list), "button_press_event", 
			   GTK_SIGNAL_FUNC(newsf_clicked), ghtlc);

	topframe = gtk_frame_new(0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);

	hbuttonbox = gtk_hbox_new(0,0);

	vbox = gtk_vbox_new(0, 0);
	gtk_widget_set_usize(vbox, 240, 400);
	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox);
	gtk_box_pack_start(GTK_BOX(vbox), topframe, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(news_scroll), news_list);
	gtk_box_pack_start(GTK_BOX(vbox), news_scroll, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(news_window), vbox);

	gfnews->window = news_window;
	gfnews->news_list = news_list;
	gtk_widget_show_all(news_window);
	keyaccel_attach(ghtlc, news_window);

	return gfnews;
}

void
output_news_dirlist (struct htlc_conn *htlc, struct news_folder *news)
{
	struct ghtlc_conn *ghtlc;
	struct gnews_folder *gfnews;
	GtkWidget *news_list;
	unsigned int row, i;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkStyle *style;
	GdkColor col = {0,0,0,0};

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	gfnews = gfnews_with_path(ghtlc, news->path);
	if (!gfnews) {
		gfnews = create_gfnews_window(ghtlc, news);
	}
	news_list = gfnews->news_list;

	style = gtk_widget_get_style(news_list);

	gtk_hlist_clear(GTK_HLIST(news_list));

	gtk_hlist_freeze(GTK_HLIST(news_list));
	for (i = 0; i < news->num_entries; i++) {
		struct folder_item *item = news->entry[i];
		gchar *nulls[2] = {0, 0};

		row = gtk_hlist_append(GTK_HLIST(news_list), nulls);
		gtk_hlist_set_row_data(GTK_HLIST(news_list), row, item);
		icon = gdk_pixmap_create_from_xpm_d(gfnews->window->window,
						    &mask,
						    &style->bg[GTK_STATE_NORMAL],
						    (item->type == 1 || item->type == 2) ? 
						    newsfld_xpm : newscat_xpm);
#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, item->name, strlen(item->name), &out_p);
	if (out_len) {
		gtk_hlist_set_pixtext(GTK_HLIST(news_list), row, 0, out_p, 34, icon, mask);
		xfree(out_p);
	} else {
		gtk_hlist_set_pixtext(GTK_HLIST(news_list), row, 0, item->name, 34, icon, mask);
	}
}
#else
		gtk_hlist_set_pixtext(GTK_HLIST(news_list), row, 0, item->name, 34, icon, mask);
#endif
		gtk_hlist_set_foreground(GTK_HLIST(news_list), row, &col);
	}
	gtk_hlist_thaw(GTK_HLIST(news_list));
}

struct gnews_catalog *
gcnews_with_path (struct ghtlc_conn *ghtlc, char *path)
{
	struct gnews_catalog *gcnews;

	for (gcnews = ghtlc->gcnews_list; gcnews; gcnews = gcnews->prev) {
		if (!strcmp(path, gcnews->group->path)) {
			return gcnews;
		}
	}
	return 0;
}

struct gnews_catalog *
gcnews_with_group (struct ghtlc_conn *ghtlc, struct news_group *group)
{
	struct gnews_catalog *gcnews;

	for (gcnews = ghtlc->gcnews_list; gcnews; gcnews = gcnews->prev) {
		if (group == gcnews->group) {
			return gcnews;
		}
	}

	return 0;
}

static void
delete_gcnews (struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc = gcnews->ghtlc;

	if (gcnews->next)
		gcnews->next->prev = gcnews->prev;
	if (gcnews->prev)
		gcnews->prev->next = gcnews->next;
	if (gcnews == ghtlc->gcnews_list)
		ghtlc->gcnews_list = gcnews->prev;
	xfree(gcnews);
}

static void
destroy_gcnews_browser (GtkWidget *widget, gpointer data)
{
	struct gnews_catalog *gcnews = (struct gnews_catalog *)data;

	delete_gcnews(gcnews);
}

static void
newsc_clicked (GtkCTree *ctree, GList *node, gint column, struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc;
	struct news_item *item = gtk_ctree_node_get_row_data(ctree, (GtkCTreeNode *)node);

	ghtlc = gcnews->ghtlc;
	hx_tnews_get(ghtlc->htlc, item->group->path, item->postid, item->parts[0].mime_type, item);
	gcnews->row = (GtkCTreeNode *)node;
}

void
news15_do_reply (GtkWidget *btn, struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *text = gtk_object_get_data(GTK_OBJECT(btn), "text");
	GtkWidget *reply = gtk_object_get_data(GTK_OBJECT(btn), "reply");
	GtkWidget *subject = gtk_object_get_data(GTK_OBJECT(btn), "subject");
	GtkWidget *window = gtk_object_get_data(GTK_OBJECT(btn), "window");
	char *textbuf = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
	u_int32_t postid = atoi(gtk_entry_get_text(GTK_ENTRY(reply)));
	char *subjectbuf = gtk_entry_get_text(GTK_ENTRY(subject));

	ghtlc = gcnews->ghtlc;
	hx_tnews_post(ghtlc->htlc, gcnews->group->path, subjectbuf,
		      postid, textbuf);

	gtk_widget_destroy(window);
}

void
news15_cancel_post(GtkWidget *btn, GtkWidget *window)
{
	gtk_widget_destroy(window);
}

void
news15_reply (GtkWidget *btn, struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc;
	struct news_item *item = 0;
	GtkWidget *window;
	GtkWidget *inreplyto;
	GtkWidget *replylbl;
	GtkWidget *subject;
	GtkWidget *subjectlbl;
	GtkWidget *text;
	GtkWidget *textlbl;
	GtkWidget *vscroll;
	GtkWidget *post, *cancel;
	GtkWidget *hbox, *vbox;
	GtkWidget *table;

	ghtlc = gcnews->ghtlc;
	if (gcnews->row) {
		item = gtk_ctree_node_get_row_data(
			GTK_CTREE(gcnews->news_tree), gcnews->row);
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize(window, 320, 250);
	gtk_window_set_title(GTK_WINDOW(window), _("Post News (1.5+)"));
	gtk_container_border_width(GTK_CONTAINER(window), 5);

	vbox = gtk_vbox_new(0,0);
	table = gtk_table_new(3, 2, 0);

	replylbl = gtk_label_new("In Reply To Post #: ");
	inreplyto = gtk_entry_new();
	if (item) {
		char *buf = g_strdup_printf("%d", item->postid);
		
		gtk_entry_set_text(GTK_ENTRY(inreplyto), buf);
		g_free(buf);
	}

	gtk_table_attach(GTK_TABLE(table), replylbl, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), inreplyto, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	subjectlbl = gtk_label_new("Subject: ");
	subject = gtk_entry_new();

	if (item) {
		if (strncasecmp(item->subject, "re:", 3)) {
			char *buf = g_strdup_printf("Re: %s", item->subject);
			gtk_entry_set_text(GTK_ENTRY(subject), buf);
			g_free(buf);
		} else {
			gtk_entry_set_text(GTK_ENTRY(subject), item->subject);
		}
	}

	gtk_table_attach(GTK_TABLE(table), subjectlbl, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), subject, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	textlbl = gtk_label_new("Body: ");
	
	gtk_table_attach(GTK_TABLE(table), textlbl, 0, 1, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	hbox = gtk_hbox_new(0, 0);
	text = gtk_text_new(0, 0);
	gtk_editable_set_editable(GTK_EDITABLE(text), 1);
	vscroll = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);

	gtk_box_pack_start(GTK_BOX(hbox), text, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vscroll, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), table, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 10);

	hbox = gtk_hbox_new(1, 0);

	post = gtk_button_new_with_label("Post");
	gtk_object_set_data(GTK_OBJECT(post), "text", text);
	gtk_object_set_data(GTK_OBJECT(post), "reply", inreplyto);
	gtk_object_set_data(GTK_OBJECT(post), "subject", subject);
	gtk_object_set_data(GTK_OBJECT(post), "window", window);
	gtk_signal_connect(GTK_OBJECT(post), "clicked", GTK_SIGNAL_FUNC(news15_do_reply), gcnews);

	cancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			GTK_SIGNAL_FUNC(news15_cancel_post), window);

	gtk_box_pack_start(GTK_BOX(hbox), post, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cancel, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);
	keyaccel_attach(ghtlc, window);
	gtk_widget_show_all(window);
}

void
news15_do_post (GtkWidget *btn, struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *text = gtk_object_get_data(GTK_OBJECT(btn), "text");
	GtkWidget *subject = gtk_object_get_data(GTK_OBJECT(btn), "subject");
	GtkWidget *window = gtk_object_get_data(GTK_OBJECT(btn), "window");
	char *textbuf = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
	char *subjectbuf = gtk_entry_get_text(GTK_ENTRY(subject));

	ghtlc = gcnews->ghtlc;
	hx_tnews_post(ghtlc->htlc, gcnews->group->path, subjectbuf, 0, textbuf);

	gtk_widget_destroy(window);
}

void
news15_post (GtkWidget *btn, struct gnews_catalog *gcnews)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *window;
	GtkWidget *subject;
	GtkWidget *subjectlbl;
	GtkWidget *text;
	GtkWidget *textlbl;
	GtkWidget *vscroll;
	GtkWidget *post, *cancel;
	GtkWidget *hbox, *vbox;
	GtkWidget *table;

	ghtlc = gcnews->ghtlc;
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize(window, 320, 250);
	gtk_window_set_title(GTK_WINDOW(window), _("Post News (1.5+)"));
	gtk_container_border_width(GTK_CONTAINER(window), 5);

	vbox = gtk_vbox_new(0,0);
	table = gtk_table_new(2, 2, 0);

	subjectlbl = gtk_label_new("Subject: ");
	subject = gtk_entry_new();

	gtk_table_attach(GTK_TABLE(table), subjectlbl, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), subject, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	textlbl = gtk_label_new("Body: ");
	
	gtk_table_attach(GTK_TABLE(table), textlbl, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	hbox = gtk_hbox_new(0, 0);
	text = gtk_text_new(0, 0);
	gtk_editable_set_editable(GTK_EDITABLE(text), 1);
	vscroll = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);

	gtk_box_pack_start(GTK_BOX(hbox), text, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vscroll, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), table, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 10);

	hbox = gtk_hbox_new(1, 0);

	post = gtk_button_new_with_label("Post");
	gtk_object_set_data(GTK_OBJECT(post), "text", text);
	gtk_object_set_data(GTK_OBJECT(post), "subject", subject);
	gtk_object_set_data(GTK_OBJECT(post), "window", window);
	gtk_signal_connect(GTK_OBJECT(post), "clicked", GTK_SIGNAL_FUNC(news15_do_post), gcnews);

	cancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			GTK_SIGNAL_FUNC(news15_cancel_post), window);

	gtk_box_pack_start(GTK_BOX(hbox), post, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cancel, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);
	keyaccel_attach(ghtlc, window);
	gtk_widget_show_all(window);
}

struct gnews_catalog *
create_gcnews_window (struct ghtlc_conn *ghtlc, struct news_group *group)
{
	struct gnews_catalog *gcnews;
	GtkWidget *news_window;
	GtkWidget *hpaned1;
	GtkWidget *vbox1;
	GtkWidget *hbuttonbox1;
	GtkWidget *refreshbtn;
	GtkWidget *postbtn;
	GtkWidget *replybtn;
	GtkWidget *alignment1;
	GtkWidget *news_tree;
	GtkWidget *vbox2;
	GtkWidget *authorlbl;
	GtkWidget *datelbl;
	GtkWidget *subjectlbl;
	GtkWidget *scrolledwindow1;
	GtkWidget *news_text;
	GtkWidget *scrolledwindow2;
	GtkWidget *viewport1;
	GtkWidget *topframe;
	GtkStyle *style;

	gcnews = xmalloc(sizeof(struct gnews_catalog));
	memset(gcnews, 0, sizeof(struct gnews_catalog));
	gcnews->ghtlc = ghtlc;
	gcnews->prev = ghtlc->gcnews_list;
	gcnews->next = 0;
	gcnews->group = group;
	if (ghtlc->gcnews_list)
		ghtlc->gcnews_list->next = gcnews;
	ghtlc->gcnews_list = gcnews;

	news_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(news_window), 1, 1, 0);
	gtk_widget_realize(news_window);
	style = gtk_widget_get_style(news_window);
	gtk_widget_set_usize(news_window, 570, 375); 
	gtk_window_set_title(GTK_WINDOW(news_window), group->path);
	gtk_object_set_data(GTK_OBJECT(news_window), "gcnews", gcnews);
	gtk_signal_connect(GTK_OBJECT(news_window), "destroy", 
			   GTK_SIGNAL_FUNC(destroy_gcnews_browser), gcnews);

	hpaned1 = gtk_hpaned_new();
	gtk_container_add(GTK_CONTAINER(news_window), hpaned1);
	gtk_container_set_border_width(GTK_CONTAINER(hpaned1), 4);
	gtk_paned_set_position(GTK_PANED (hpaned1), 285);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_paned_pack1(GTK_PANED (hpaned1), vbox1, FALSE, TRUE);

	topframe = gtk_frame_new(0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(vbox1), topframe, 0, 0, 0);

	hbuttonbox1 = gtk_hbutton_box_new();
	/* gtk_box_pack_start(GTK_BOX(vbox1), hbuttonbox1, FALSE, FALSE, 0); */
	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox1);

	refreshbtn = gtk_button_new_with_label("Refresh");
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), refreshbtn);

	postbtn = gtk_button_new_with_label("Post");
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), postbtn);
	gtk_signal_connect(GTK_OBJECT(postbtn), "clicked",
			   GTK_SIGNAL_FUNC(news15_post), gcnews);

	replybtn = gtk_button_new_with_label("Reply");
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), replybtn);
	gtk_signal_connect(GTK_OBJECT(replybtn), "clicked",
			   GTK_SIGNAL_FUNC(news15_reply), gcnews);

/*	GTK_WIDGET_SET_FLAGS(refreshbtn, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(postbtn, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(replybtn, GTK_CAN_DEFAULT); */

	alignment1 = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_box_pack_start(GTK_BOX(vbox1), alignment1, TRUE, TRUE, 0);

	scrolledwindow2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(alignment1), scrolledwindow2);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	viewport1 = gtk_viewport_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolledwindow2), viewport1);
	
	news_tree = gtk_ctree_new(1, 0);
	gtk_clist_set_row_height(GTK_CLIST(news_tree), 18); 
	gtk_clist_set_shadow_type(GTK_CLIST(news_tree), GTK_SHADOW_NONE);
	gtk_signal_connect(GTK_OBJECT(news_tree), "tree_select_row", 
					   GTK_SIGNAL_FUNC(newsc_clicked), gcnews);
	gtk_container_add(GTK_CONTAINER(viewport1), news_tree);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_paned_pack2(GTK_PANED (hpaned1), vbox2, TRUE, TRUE);

	authorlbl = gtk_label_new("Author: ");
	gtk_label_set_justify(GTK_LABEL(authorlbl), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox2), authorlbl, 0, 1, 0);

	datelbl = gtk_label_new("Date: ");
	gtk_label_set_justify(GTK_LABEL(datelbl), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox2), datelbl, 0, 1, 0);
	
	subjectlbl = gtk_label_new("Subject: ");
	gtk_label_set_justify(GTK_LABEL(subjectlbl), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox2), subjectlbl, 0, 1, 0);
	
	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox2), scrolledwindow1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	news_text = gtk_text_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), news_text);
	gtk_widget_show_all(news_window);

	gcnews->window = news_window;
	gcnews->news_tree = news_tree;
	gcnews->news_text = news_text;
	gcnews->subjectlbl = subjectlbl;
	gcnews->datelbl = datelbl;
	gcnews->authorlbl = authorlbl;

	keyaccel_attach(ghtlc, news_window);

	return gcnews;
}

void
output_news_catlist (struct htlc_conn *htlc, struct news_group *group)
{
	struct ghtlc_conn *ghtlc;
	struct gnews_catalog *gcnews;
	GtkCTreeNode *parent;
	int i;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	gcnews = gcnews_with_path(ghtlc, group->path);
	if (!gcnews) {
		if (!(gcnews = create_gcnews_window(ghtlc, group))) {
			return;
		}
	}

	gtk_clist_clear(GTK_CLIST(gcnews->news_tree));
	gtk_clist_freeze(GTK_CLIST(gcnews->news_tree));
	for (i = 0; i < group->post_count; i++) {
		struct news_item *item = &(group->posts[i]);
		int j;
		parent = 0;

		if (!item)
			continue;
		for (j = 0; j < group->post_count; j++) {
			if (group->posts[j].postid == item->parentid) {
				parent = (GtkCTreeNode *)group->posts[j].node;
				break;
			}
		}

#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, item->subject, strlen(item->subject), &out_p);
	if (out_len) {
		item->node = gtk_ctree_insert_node(GTK_CTREE(gcnews->news_tree), 
						   parent, 0, &out_p, 0, 0, 0,
						   0, 0, 0, 0);
		xfree(out_p);
	} else {
		item->node = gtk_ctree_insert_node(GTK_CTREE(gcnews->news_tree), 
						   parent, 0, &item->subject, 0, 0, 0,
						   0, 0, 0, 0);
	}
}
#else
		item->node = gtk_ctree_insert_node(GTK_CTREE(gcnews->news_tree), 
						   parent, 0, &item->subject, 0, 0, 0,
						   0, 0, 0, 0);
#endif
		gtk_ctree_node_set_row_data(GTK_CTREE(gcnews->news_tree), (GtkCTreeNode *)item->node, item);
	}
	gtk_clist_thaw(GTK_CLIST(gcnews->news_tree));
}
 
static time_t
date_to_unix (struct date_time *dt)
{
	/* check if the year is after the epoch */
	if (dt->base_year >= 1970) {
		struct tm timetm;
		time_t timet;
		
		memset(&timetm, 0, sizeof(struct tm));
		/* the 24*3600 thing is a hack for a weird bug
			where the date would be displayed one day behind from
			the actual date ... quite odd */
		timetm.tm_sec = dt->seconds+(24*3600);
		timetm.tm_year = (dt->base_year-1900);
		timet =  mktime(&timetm);	
		return timet;
	} else {
		/* crackhead base_year detected */
		if (dt->base_year == 1904)
 			return dt->seconds-2082844800U;
	}
	
	return 0;
}

void
output_news_thread (struct htlc_conn *htlc, struct news_post *post)
{
	struct ghtlc_conn *ghtlc;
	struct gnews_catalog *gcnews;
	struct news_item *item;
	time_t timet;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	item = post->item;
	gcnews = gcnews_with_group(ghtlc, item->group);
	if (!gcnews) {
		return;
	}

	timet = date_to_unix(&item->date);

	gtk_editable_delete_text(GTK_EDITABLE(gcnews->news_text), 0, -1);
	if (item) {
		char *date = g_strdup_printf("Date: %s", ctime(&timet));

		/* get rid of the line break ctime() adds in */
		date[strlen(date)-1] = '\0';

		/* preliminary post thread displaying code */
		if (item->sender) {
			char *str = g_strdup_printf("Author: %s", item->sender);
			gtk_label_set_text(GTK_LABEL(gcnews->authorlbl), str);
			g_free(str);
		}
		if (item->subject) {
			char *str = g_strdup_printf("Subject: %s", item->subject);
#if defined(CONFIG_ICONV)
{
			char *out_p;
			size_t out_len;

			out_len = convbuf(g_encoding, g_server_encoding, str, strlen(str), &out_p);
			if (out_len) {
				gtk_label_set_text(GTK_LABEL(gcnews->subjectlbl), out_p);
				xfree(out_p);
			}
}
#else
			gtk_label_set_text(GTK_LABEL(gcnews->subjectlbl), str);
#endif
			g_free(str);
		}
		gtk_label_set_text(GTK_LABEL(gcnews->datelbl), date);
		g_free(date);
	}

#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, post->buf, post->buflen, &out_p);
	if (out_len) {
		gtk_text_insert(GTK_TEXT(gcnews->news_text), 0, 0, 0, out_p, out_len);
		xfree(out_p);
	}
}
#else
	/* output the contents of the post */
 	gtk_text_insert(GTK_TEXT(gcnews->news_text), 0, 0, 0, post->buf, strlen(post->buf));
#endif
}

void
open_tnews (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	char *path;

	path = xstrdup("/");
	//task_new(ghtlc->htlc, rcv_task_newsfolder_list, path, 0, "news_folder");
	hx_tnews_list(ghtlc->htlc, path, 0);
}
