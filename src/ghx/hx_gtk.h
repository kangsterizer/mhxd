#ifndef __hx_gtk_h
#define __hx_gtk_h

#include "hxd.h"
#include <gtk/gtk.h>

struct ghtlc_conn;

struct ghx_window {
	struct ghx_window *next, *prev;
	struct ghtlc_conn *ghtlc;
	unsigned int wgi;
	struct window_geometry *wg;
	GtkWidget *widget;
};

struct gchat;
struct msgchat;
struct gtask;
struct gfile_list;
struct tracker_server;
struct gnews_folder;
struct gnews_catalog;

struct ghtlc_conn {
	struct ghtlc_conn *next, *prev;
	struct htlc_conn *htlc;
	unsigned int connected;
	struct ghx_window *window_list;

	GtkWidget *news_text;
	GtkWidget *preview_hbox;

	struct gchat *gchat_list;

	struct msgchat *msgchat_list;

	struct gnews_folder *gfnews_list;
	struct gnews_catalog *gcnews_list;

	GtkWidget *users_list;
	GtkStyle *users_style;
	GdkFont *users_font;
	GdkFont *chat_font;

	struct gtask *gtask_list;
	GtkWidget *gtask_gtklist;

	struct gfile_list *gfile_list;

	GtkWidget *tracker_list;
	struct tracker_server *tracker_server_list;
	struct tracker_server *tracker_server_tail;

	GtkWidget *toolbar_hbox;
	GtkWidget *connectbtn, *disconnectbtn;
	GtkWidget *chatbtn, *filesbtn, *newsbtn, *tnewsbtn, *tasksbtn, *closebtn, *quitbtn;
	GtkWidget *usersbtn, *usereditbtn, *trackerbtn, *optionsbtn, *aboutbtn;
	GtkWidget *user_msgbtn, *user_infobtn, *user_kickbtn, *user_banbtn, *user_chatbtn;
	GtkWidget *news_postbtn, *news_reloadbtn;
};

struct gchat {
	struct gchat *next, *prev;
	struct ghtlc_conn *ghtlc;
	struct hx_chat *chat;
	struct ghx_window *gwin;
	unsigned int do_lf;
	void *chat_history;
	GtkWidget *chat_input_text;
	GtkWidget *chat_output_text;
	GtkWidget *chat_vscrollbar;
	GtkWidget *chat_in_hbox;
	GtkWidget *chat_out_hbox;
	GtkWidget *subject_hbox;
	GtkWidget *subject_entry;
	GtkWidget *users_list;
	GtkWidget *users_vbox;
};

struct gnews_folder {
	struct gnews_folder *next, *prev;
	struct ghtlc_conn *ghtlc;
	GtkWidget *window;
	GtkWidget *news_list;
	gint row, col;
	struct news_folder *news;
};

struct gnews_catalog {
	struct gnews_catalog *next, *prev;
	struct ghtlc_conn *ghtlc;
	struct news_group *group;
	GtkWidget *window;
	GtkWidget *news_tree;
	GtkWidget *news_text;
	GtkWidget *authorlbl, *subjectlbl, *datelbl;
	GtkCTreeNode *row;
};

struct window_geometry {
	u_int16_t width, height;
	int16_t xpos, ypos;
	int16_t xoff, yoff;
};

#define NWG		12
#define WG_CHAT		0
#define WG_TOOLBAR	1
#define WG_TASKS	2
#define WG_USERS	3
#define WG_NEWS		4
#define WG_POST		5
#define WG_TRACKER	6
#define WG_OPTIONS	7
#define WG_USEREDIT	8
#define WG_CONNECT	9
#define WG_FILES	10
#define WG_PREVIEW	11

#define SCROLLBAR_SPACING(w) (GTK_SCROLLED_WINDOW_CLASS(GTK_OBJECT(w)->klass)->scrollbar_spacing)

#define CM_NENTRIES(cme)	(sizeof(cme)/sizeof(struct context_menu_entry))

extern struct ghtlc_conn *ghtlc_conn_with_htlc (struct htlc_conn *htlc);
extern struct ghtlc_conn *ghtlc_conn_new (struct htlc_conn *htlc);
extern void ghtlc_conn_delete (struct ghtlc_conn *ghtlc);

extern void keyaccel_attach (struct ghtlc_conn *ghtlc, GtkWidget *widget);

extern struct ghx_window *window_create (struct ghtlc_conn *ghtlc, unsigned int wgi);
extern struct ghx_window *ghx_window_with_wgi (struct ghtlc_conn *ghtlc, unsigned int wgi);
extern void changetitle (struct ghtlc_conn *ghtlc, GtkWidget *window, char *nam);
extern void window_present (GtkWidget *win);
extern void ghx_window_present (struct ghx_window *gwin);

extern GdkColor *colorgdk (u_int16_t color);

extern char *def_font;
extern char *def_users_font;

#endif /* __hx_gtk_h */
