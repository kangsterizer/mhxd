#include "hx.h"
#include "hx_gtk.h"
#include <fnmatch.h>
#include "gtk_hlist.h"
#include "xmalloc.h"
#include <string.h>

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

struct connect_context;

extern void create_chat_window (struct ghtlc_conn *ghtlc, struct hx_chat *chat);
extern void create_toolbar_window (struct ghtlc_conn *ghtlc);
extern void connect_set_entries (struct connect_context *cc, const char *address, const char *login, const char *password);
extern struct connect_context *create_connect_window (struct ghtlc_conn *ghtlc);

struct tracker_server {
	void *next;
	char *addrstr;
	char *name;
	char *desc;
	u_int16_t port;
	u_int16_t nusers;
};

struct generic_list {
	void *next;
};

static void
list_free (void *listp)
{
	struct generic_list *lp, *next;

	for (lp = (struct generic_list *)listp; lp; lp = next) {
		next = lp->next;
		xfree(lp);
	}
}

static void
tracker_destroy (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	ghtlc->tracker_list = 0;
	if (ghtlc->tracker_server_list) {
		list_free(ghtlc->tracker_server_list);
		ghtlc->tracker_server_list = 0;
	}
}

static void
tracker_getlist (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *hostentry = gtk_object_get_data(GTK_OBJECT(widget), "hostentry");
	char *host;

	gtk_hlist_clear(GTK_HLIST(ghtlc->tracker_list));
	if (ghtlc->tracker_server_list) {
		list_free(ghtlc->tracker_server_list);
		ghtlc->tracker_server_list = 0;
	}
	host = gtk_entry_get_text(GTK_ENTRY(hostentry));
	hx_tracker_list(ghtlc->htlc, 0, host, HTRK_TCPPORT);
}

static void
tracker_search (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *tracker_list = ghtlc->tracker_list;
	char *str;
	struct tracker_server *server;
	gint row;
	gchar *text[5];
	char nusersstr[8], portstr[8];

	str = gtk_entry_get_text(GTK_ENTRY(widget));
	if (!str || !*str)
		return;
	str = g_strdup_printf("*%s*", str);
	gtk_hlist_freeze(GTK_HLIST(tracker_list));
	gtk_hlist_clear(GTK_HLIST(tracker_list));
	for (server = ghtlc->tracker_server_list; server; server = server->next) {
#if !defined(FNM_CASEFOLD)
#define FNM_CASEFOLD 0
#endif
		if (!fnmatch(str, server->desc, FNM_NOESCAPE|FNM_CASEFOLD)
		    || !fnmatch(str, server->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
			snprintf(nusersstr, sizeof(nusersstr), "%u", server->nusers);
			snprintf(portstr, sizeof(portstr), "%u", server->port);
			text[0] = server->name;
			text[1] = nusersstr;
			text[2] = server->addrstr;
			text[3] = portstr;
			text[4] = server->desc;
			row = gtk_hlist_append(GTK_HLIST(tracker_list), text);
			gtk_hlist_set_row_data(GTK_HLIST(tracker_list), row, server);
		}
	}
	gtk_hlist_thaw(GTK_HLIST(tracker_list));
	g_free(str);
}

void
tracker_server_create (struct htlc_conn *htlc,
		       const char *addrstr, u_int16_t port, u_int16_t nusers,
		       const char *nam, const char *desc)
{
	GtkWidget *encentry;
	struct ghtlc_conn *ghtlc;
	GtkWidget *tracker_list;
	gint row;
	struct tracker_server *server;
	char nusersstr[8], portstr[8];
	gchar *text[5];
	char *enc;
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;
#endif

	ghtlc = ghtlc_conn_with_htlc(htlc);
	tracker_list = ghtlc->tracker_list;
	if (!tracker_list)
		return;
	encentry = gtk_object_get_data(GTK_OBJECT(tracker_list), "encentry");
	enc = gtk_entry_get_text(GTK_ENTRY(encentry));

	server = xmalloc(sizeof(struct tracker_server));
	server->next = 0;
	if (!ghtlc->tracker_server_list) {
		ghtlc->tracker_server_list = server;
		ghtlc->tracker_server_tail = server;
	} else {
		ghtlc->tracker_server_tail->next = server;
		ghtlc->tracker_server_tail = server;
	}

	server->addrstr = xstrdup(addrstr);
	server->port = port;
	server->nusers = nusers;
#if defined(CONFIG_ICONV)
	out_len = convbuf(g_encoding, enc, nam, strlen(nam), &out_p);
	if (out_len) {
		server->name = xmalloc(out_len+1);
		memcpy(server->name, out_p, out_len);
		server->name[out_len] = 0;
		xfree(out_p);
	} else {
		server->name = xstrdup(nam);
	}
	out_len = convbuf(g_encoding, enc, desc, strlen(desc), &out_p);
	if (out_len) {
		server->desc = xmalloc(out_len+1);
		memcpy(server->desc, out_p, out_len);
		server->desc[out_len] = 0;
		xfree(out_p);
	} else {
		server->desc = xstrdup(desc);
	}
#else
	server->name = xstrdup(nam);
	server->desc = xstrdup(desc);
#endif

	snprintf(nusersstr, sizeof(nusersstr), "%u", server->nusers);
	snprintf(portstr, sizeof(portstr), "%u", server->port);
	text[0] = server->name;
	text[1] = nusersstr;
	text[2] = server->addrstr;
	text[3] = portstr;
	text[4] = server->desc;
	row = gtk_hlist_append(GTK_HLIST(tracker_list), text);
	gtk_hlist_set_row_data(GTK_HLIST(tracker_list), row, server);
}

static int tracker_storow;
static int tracker_stocol;

static gint
tracker_click (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *tracker_list = ghtlc->tracker_list;
	struct htlc_conn *htlc;

	gtk_hlist_get_selection_info(GTK_HLIST(widget), event->x, event->y,
				     &tracker_storow, &tracker_stocol);
	if (event->type == GDK_2BUTTON_PRESS) {
		struct tracker_server *server;

		htlc = xmalloc(sizeof(struct htlc_conn));
		memset(htlc, 0, sizeof(struct htlc_conn));
		strcpy(htlc->name, ghtlc->htlc->name);
		htlc->icon = ghtlc->htlc->icon;
		ghtlc = ghtlc_conn_new(htlc);
		create_toolbar_window(ghtlc);
		create_chat_window(ghtlc, 0);

		server = gtk_hlist_get_row_data(GTK_HLIST(tracker_list), tracker_storow);
		hx_connect(htlc, server->addrstr, server->port, htlc->name, htlc->icon, 0, 0, 0);
		return 1;
	}

	return 0;
}

static void
tracker_connect (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct tracker_server *server;

	server = gtk_hlist_get_row_data(GTK_HLIST(ghtlc->tracker_list), tracker_storow);
	if (server) {
		char addrstr[32];
		struct connect_context *cc;

		cc = create_connect_window(ghtlc);
		snprintf(addrstr, sizeof(addrstr), "%s:%u", server->addrstr, server->port);
		connect_set_entries(cc, addrstr, 0, 0);
	}
}

void
create_tracker_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *tracker_window;
	GtkWidget *tracker_list;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hostlabel;
	GtkWidget *hostentry;
	GtkWidget *enclabel;
	GtkWidget *encentry;
	GtkWidget *searchhbox;
	GtkWidget *searchlabel;
	GtkWidget *searchentry;
	GtkWidget *tracker_window_scroll;
	GtkWidget *refreshbtn;
	GtkWidget *connbtn;
	GtkWidget *optionsbtn;
	static gchar *titles[] = {"Name", "Users", "Address", "Port", "Description"};

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_TRACKER))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_TRACKER);
	wg = gwin->wg;
	tracker_window = gwin->widget;

	gtk_window_set_title(GTK_WINDOW(tracker_window), "Tracker");
	gtk_signal_connect_object(GTK_OBJECT(tracker_window), "destroy",
				  GTK_SIGNAL_FUNC(tracker_destroy), (gpointer)ghtlc);

	tracker_list = gtk_hlist_new_with_titles(5, titles);
	ghtlc->tracker_list = tracker_list;
	gtk_widget_set_usize(tracker_list, wg->width, wg->height-60);
	gtk_hlist_set_column_width(GTK_HLIST(tracker_list), 0, 160);
	gtk_hlist_set_column_width(GTK_HLIST(tracker_list), 1, 48);
	gtk_hlist_set_column_justification(GTK_HLIST(tracker_list), 1, GTK_JUSTIFY_CENTER);
	gtk_hlist_set_column_width(GTK_HLIST(tracker_list), 2, 144);
	gtk_hlist_set_column_width(GTK_HLIST(tracker_list), 3, 64);
	gtk_hlist_set_column_width(GTK_HLIST(tracker_list), 4, 256);
	gtk_signal_connect(GTK_OBJECT(tracker_list), "button_press_event",
			   GTK_SIGNAL_FUNC(tracker_click), ghtlc);

	hostlabel = gtk_label_new("Address:");
	hostentry = gtk_entry_new();
	gtk_object_set_data(GTK_OBJECT(hostentry), "hostentry", hostentry);
	gtk_signal_connect(GTK_OBJECT(hostentry), "activate",
			   GTK_SIGNAL_FUNC(tracker_getlist), ghtlc);

	enclabel = gtk_label_new("Encoding:");
	encentry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(encentry), g_server_encoding);
	gtk_object_set_data(GTK_OBJECT(tracker_list), "encentry", encentry);

	searchlabel = gtk_label_new("Search:");
	searchentry = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(searchentry), "activate",
			   GTK_SIGNAL_FUNC(tracker_search), ghtlc);

	refreshbtn = gtk_button_new_with_label("Refresh");
	gtk_object_set_data(GTK_OBJECT(refreshbtn), "hostentry", hostentry);
	gtk_signal_connect(GTK_OBJECT(refreshbtn), "clicked",
			   GTK_SIGNAL_FUNC(tracker_getlist), ghtlc);
	connbtn = gtk_button_new_with_label("Connect");
	gtk_signal_connect_object(GTK_OBJECT(connbtn), "clicked",
				  GTK_SIGNAL_FUNC(tracker_connect), (gpointer)ghtlc);
	optionsbtn = gtk_button_new_with_label("Options");

	tracker_window_scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(tracker_window_scroll) = 0;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tracker_window_scroll),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(tracker_window_scroll, wg->width, wg->height-50);
	gtk_container_add(GTK_CONTAINER(tracker_window_scroll), tracker_list);

	vbox = gtk_vbox_new(0, 0);
	gtk_widget_set_usize(vbox, wg->height, wg->width);
	hbox = gtk_hbox_new(0, 0);
	searchhbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), refreshbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), connbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), optionsbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hostlabel, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hostentry, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), enclabel, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), encentry, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(searchhbox), searchlabel, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(searchhbox), searchentry, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(vbox), searchhbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), tracker_window_scroll, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(tracker_window), vbox);

	gtk_widget_show_all(tracker_window);
}

void
open_tracker (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_tracker_window(ghtlc);
}
