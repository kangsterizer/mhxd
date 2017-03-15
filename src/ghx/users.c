#include "hx.h"
#include "hx_gtk.h"
#include <gdk/gdkkeysyms.h>
#include "gtk_hlist.h"
#include "xmalloc.h"
#include "icons.h"
#include "cmenu.h"
#include <stdio.h>
#include <string.h>

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#define MSG_FROM_COLOR	(&colors[9])	/* red */
#define MSG_TO_COLOR	(&colors[12])	/* blue */

extern GdkColor colors[16];

extern GdkGC *users_gc;

extern void create_useredit_window (struct ghtlc_conn *ghtlc);
extern struct gchat *gchat_with_cid (struct ghtlc_conn *ghtlc, u_int32_t cid);
extern struct gchat *gchat_with_users_list (struct ghtlc_conn *ghtlc, GtkWidget *users_list);
extern struct gchat *gchat_with_chat (struct ghtlc_conn *ghtlc, struct hx_chat *chat);
extern void create_chat_window (struct ghtlc_conn *ghtlc, struct hx_chat *chat);

struct msgchat {
	struct msgchat *next, *prev;
	struct ghtlc_conn *ghtlc;
	GtkWidget *from_text;
	GtkWidget *to_text;
	GtkWidget *from_hbox;
	GtkWidget *to_hbox;
	GtkWidget *win;
	GtkWidget *vpane;
	GtkWidget *chat_chk;
	u_int32_t uid;
	unsigned int chat;
	u_int8_t name[32];
};

static struct msgchat *
msgchat_new (struct ghtlc_conn *ghtlc, u_int32_t uid)
{
	struct msgchat *msgchat;

	msgchat = xmalloc(sizeof(struct msgchat));
	memset(msgchat, 0, sizeof(struct msgchat));
	msgchat->next = 0;
	msgchat->prev = ghtlc->msgchat_list;
	if (ghtlc->msgchat_list)
		ghtlc->msgchat_list->next = msgchat;
	ghtlc->msgchat_list = msgchat;
	msgchat->ghtlc = ghtlc;
	msgchat->uid = uid;

	return msgchat;
}

static void
msgchat_delete (struct ghtlc_conn *ghtlc, struct msgchat *msgchat)
{
	if (msgchat->next)
		msgchat->next->prev = msgchat->prev;
	if (msgchat->prev)
		msgchat->prev->next = msgchat->next;
	if (msgchat == ghtlc->msgchat_list)
		ghtlc->msgchat_list = msgchat->prev;
	xfree(msgchat);
}

static struct msgchat *
msgchat_with_uid (struct ghtlc_conn *ghtlc, u_int32_t uid)
{
	struct msgchat *msgchat;

	for (msgchat = ghtlc->msgchat_list; msgchat; msgchat = msgchat->prev) {
		if (msgchat->uid == uid)
			return msgchat;
	}

	return 0;
}

static void
destroy_msgchat (gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;

	msgchat_delete(mc->ghtlc, mc);
}

static void
users_send_message (gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	struct ghtlc_conn *ghtlc = mc->ghtlc;
	GtkWidget *msgtext = mc->to_text;
	GtkWidget *win = mc->win;
	u_int32_t uid = mc->uid;
	char *msgbuf;

	msgbuf = gtk_editable_get_chars(GTK_EDITABLE(msgtext), 0, -1);
	hx_send_msg(ghtlc->htlc, uid, msgbuf, strlen(msgbuf), 0);
	g_free(msgbuf);

	gtk_widget_destroy(win);
}

static GtkWidget *
user_pixmap (GtkWidget *widget, GdkFont *font, GdkColor *color,
	     u_int16_t icon, const char *nam)
{
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GtkWidget *gtkpixmap;

	pixc = load_icon(widget, icon, &user_icon_files, 0, 0);
	if (!pixc)
		return 0;
	pixmap = gdk_pixmap_new(widget->window, 232, 18, pixc->depth);
	gdk_window_copy_area(pixmap, users_gc, 0, 0, pixc->pixmap,
			     0, 0, pixc->width, pixc->height);
	gdk_gc_set_foreground(users_gc, color);
	gdk_draw_string(pixmap, font, users_gc, 34, 13, nam);

	gtkpixmap = gtk_pixmap_new(pixmap, 0);

	return gtkpixmap;
}

static void
msgchat_input_activate (GtkWidget *widget, gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	GtkText *text;
	guint point, len;
	gchar *chars = 0;
	size_t slen = 0;

	text = GTK_TEXT(widget);
	point = gtk_text_get_point(text);
	len = gtk_text_get_length(text);
	if (len) {
		chars = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
		slen = strlen(chars);
		hx_send_msg(mc->ghtlc->htlc, mc->uid, chars, slen, 0);
	}

	gtk_text_set_point(text, 0);
	gtk_text_forward_delete(text, len);
	gtk_text_set_editable(text, 1);

	text = GTK_TEXT(mc->from_text);
	point = gtk_text_get_length(text);
	gtk_text_set_point(text, point);
	if (point)
		gtk_text_insert(text, 0, MSG_FROM_COLOR, 0, "\n", 1);
	if (chars) {
		gtk_text_insert(text, 0, MSG_TO_COLOR, 0, chars, slen);
		g_free(chars);
	}
}

static gint
msgchat_input_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	GtkText *text;
	guint point, len;
	guint k;

	if (!mc->chat)
		return 0;

	k = event->keyval;
	if (k == GDK_Return) {
		text = GTK_TEXT(widget);
		point = gtk_editable_get_position(GTK_EDITABLE(text));
		len = gtk_text_get_length(text);
		if (len == point)
			gtk_text_set_editable(text, 0);
		return 1;
	}

	return 0;
}

static void
chat_chk_activate (GtkWidget *widget, gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	GtkWidget *msgtext = mc->to_text;

	if (mc->chat == GTK_TOGGLE_BUTTON(widget)->active)
		return;
	mc->chat = GTK_TOGGLE_BUTTON(widget)->active;
	if (!msgtext)
		return;
	if (mc->chat) {
		gtk_signal_connect(GTK_OBJECT(msgtext), "key_press_event",
				   GTK_SIGNAL_FUNC(msgchat_input_key_press), mc);
		gtk_signal_connect(GTK_OBJECT(msgtext), "activate",
				   GTK_SIGNAL_FUNC(msgchat_input_activate), mc);
		gtk_widget_set_usize(mc->from_hbox, 300, 180);
		gtk_widget_set_usize(mc->to_hbox, 300, 40);
	} else {
		gtk_signal_disconnect_by_func(GTK_OBJECT(msgtext),
					      GTK_SIGNAL_FUNC(msgchat_input_activate), mc);
		gtk_widget_set_usize(mc->from_hbox, 300, 100);
		gtk_widget_set_usize(mc->to_hbox, 300, 120);
	}
}

static void
msgwin_chat (gpointer data)
{ 
	struct msgchat *mc = (struct msgchat *)data;
	struct ghtlc_conn *ghtlc = mc->ghtlc;
	u_int32_t uid = mc->uid;

	hx_chat_user(ghtlc->htlc, uid);
}

static void
msgwin_get_info (gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	struct ghtlc_conn *ghtlc = mc->ghtlc;
	u_int32_t uid = mc->uid;

	hx_get_user_info(ghtlc->htlc, uid, 0);
}

static void
users_open_message (struct ghtlc_conn *ghtlc, u_int32_t uid, const char *nam)
{
	GtkWidget *msgwin;
	GtkWidget *thbox1;
	GtkWidget *thbox2;
	GtkWidget *vbox1;
	GtkWidget *hbox2;
	GtkWidget *infobtn;
	GtkWidget *chatbtn;
	GtkWidget *msgtext;
	GtkWidget *vscrollbar;
	GtkWidget *hbox1;
	GtkWidget *chat_btn;
	GtkWidget *sendbtn;
	GtkWidget *okbtn;
	GtkWidget *pixmap;
	GtkWidget *vpane;
	GtkTooltips *tooltips;
	char title[64];
	struct gchat *gchat;
	struct hx_user *user;
	u_int16_t icon;
	struct msgchat *mc;

	mc = msgchat_with_uid(ghtlc, uid);
	if (mc) {
		window_present(mc->win);
		return;
	}

	snprintf(title, sizeof(title), "To: %s (%u)", nam, uid);

	msgwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(msgwin), 1, 1, 0);
	gtk_window_set_title(GTK_WINDOW(msgwin), "msgwin");
	gtk_window_set_default_size(GTK_WINDOW(msgwin), 300, 300);
	gtk_window_set_title(GTK_WINDOW(msgwin), title);

	mc = msgchat_new(ghtlc, uid);
	mc->ghtlc = ghtlc;
	mc->win = msgwin;
	strcpy(mc->name, nam);
	gtk_signal_connect_object(GTK_OBJECT(msgwin), "destroy",
				  GTK_SIGNAL_FUNC(destroy_msgchat), (gpointer)mc);

	vbox1 = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(msgwin), vbox1);

	hbox2 = gtk_hbox_new(0, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox2, 0, 0, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox2), 5);

	tooltips = gtk_tooltips_new();

	infobtn = icon_button_new(ICON_INFO, "Get Info", msgwin, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(infobtn), "clicked",
				  GTK_SIGNAL_FUNC(msgwin_get_info), (gpointer)mc); 
	gtk_box_pack_start(GTK_BOX(hbox2), infobtn, 0, 0, 0);

	chatbtn = icon_button_new(ICON_CHAT, "Chat", msgwin, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(chatbtn), "clicked",
				  GTK_SIGNAL_FUNC(msgwin_chat), (gpointer)mc);

	gtk_box_pack_start(GTK_BOX(hbox2), chatbtn, 0, 0, 0);

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev)
		if (!gchat->prev)
			break;

	if (gchat && gchat->chat) {
		user = hx_user_with_uid(gchat->chat->user_list, uid);
		if (user) {
			icon = user->icon;
			gtk_widget_realize(msgwin);
			pixmap = user_pixmap(msgwin, ghtlc->users_font, colorgdk(user->color), icon, nam);
			if (pixmap)
				gtk_box_pack_start(GTK_BOX(hbox2), pixmap, 0, 1, 0);
		} 
	}

	msgtext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(msgtext), 0);
	mc->from_text = msgtext;
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(msgtext)->vadj);
	thbox1 = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(thbox1), msgtext, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(thbox1), vscrollbar, 0, 0, 0);

	msgtext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(msgtext), 1);
	mc->to_text = msgtext;
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(msgtext)->vadj);
	thbox2 = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(thbox2), msgtext, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(thbox2), vscrollbar, 0, 0, 0);

	mc->from_hbox = thbox1;
	mc->to_hbox = thbox2;
	gtk_widget_set_usize(mc->from_hbox, 300, 40);
	gtk_widget_set_usize(mc->to_hbox, 300, 260);

	vpane = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(vpane), thbox1);
	gtk_paned_add2(GTK_PANED(vpane), thbox2);
	gtk_box_pack_start(GTK_BOX(vbox1), vpane, 1, 1, 0);
	mc->vpane = vpane;

	hbox1 = gtk_hbox_new(0, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, 0, 0, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);

	chat_btn = gtk_check_button_new_with_label("Chat");
	gtk_signal_connect(GTK_OBJECT(chat_btn), "clicked",
			   GTK_SIGNAL_FUNC(chat_chk_activate), mc);
	gtk_widget_set_usize(chat_btn, 55, -2);

	sendbtn = gtk_button_new_with_label("Send and Close");
	gtk_signal_connect_object(GTK_OBJECT(sendbtn), "clicked",
				  GTK_SIGNAL_FUNC(users_send_message), (gpointer)mc);
	gtk_widget_set_usize(sendbtn, 110, -2);

	okbtn = gtk_button_new_with_label("Dismiss");
	gtk_widget_set_usize(okbtn, 55, -2);
	gtk_signal_connect_object(GTK_OBJECT(okbtn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(msgwin));

	gtk_box_pack_start(GTK_BOX(hbox1), chat_btn, 0, 0, 0);
	gtk_box_pack_end(GTK_BOX(hbox1), sendbtn, 0, 0, 0);
	gtk_box_pack_end(GTK_BOX(hbox1), okbtn, 0, 0, 0);

	gtk_widget_show_all(msgwin);

	keyaccel_attach(ghtlc, msgwin);
}

static void
user_message_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		users_open_message(gchat->ghtlc, user->uid, user->name);
	}
}

static void
user_info_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_get_user_info(gchat->ghtlc->htlc, user->uid, 0);
	}
}

static void
user_kick_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_kick_user(gchat->ghtlc->htlc, user->uid, 0);
	}
}

static void
user_ban_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_kick_user(gchat->ghtlc->htlc, user->uid, 1);
	}
}

static void
user_chat_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_chat_user(gchat->ghtlc->htlc, user->uid);
	}
}

static void
user_invite_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct gchat *bgchat;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	bgchat = gchat_with_cid(gchat->ghtlc, 0);
	users_list = bgchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_chat_invite(gchat->ghtlc->htlc, gchat->chat->cid, user->uid);
	}
}

static void
user_edit_btn (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct hx_user *user;
	GtkWidget *users_list;
	GList *lp;
	gint row;

	users_list = gchat->users_list;
	if (!users_list)
		return;
	for (lp = GTK_HLIST(users_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		user = gtk_hlist_get_row_data(GTK_HLIST(users_list), row);
		if (!user)
			return;
		hx_get_user_info(gchat->ghtlc->htlc, user->uid, 0);
		create_useredit_window(gchat->ghtlc);
	}
}

static struct context_menu_entry user_menu_entries[] = {
	{ "get info", user_info_btn, 0, 0, 0, 0 },
	{ "message", user_message_btn, 0, 0, 0, 0 },
	{ "chat", user_chat_btn, 0, 0, 0, 0 },
	{ "invite", 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },
	{ "kick", user_kick_btn, 0, 0, 0, 0 },
	{ "ban", user_ban_btn, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },
	{ "edit account", user_edit_btn, 0, 0, 0, 0 },
};

static struct context_menu *user_menu;
static struct context_menu *invite_menu;

static struct context_menu *
invite_menu_new (struct ghtlc_conn *ghtlc)
{
	struct context_menu *cmenu;
	struct context_menu_entry *cme, *cmep;
	struct gchat *gchat;
	unsigned int nchats = 0;

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (gchat->chat && gchat->chat->cid)
			nchats++;
	}
	if (!nchats)
		return 0;
	cme = xmalloc(nchats * sizeof(struct context_menu_entry));
	cmep = cme;
	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (!(gchat->chat && gchat->chat->cid))
			continue;
		cmep->name = g_strdup_printf("0x%x | %s", gchat->chat->cid, gchat->chat->subject);
		cmep->signal_func = user_invite_btn;
		cmep->data = gchat;
		cmep->submenu = 0;
		cmep->menuitem = 0;
		cmep->hid = 0;
		cmep++;
	}
	cmenu = context_menu_new(cme, nchats);
	cmep = cme;
	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (!(gchat->chat && gchat->chat->cid))
			continue;
		g_free(cmep->name);
		cmep++;
	}
	xfree(cme);

	return cmenu;
}

static gint
user_clicked (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	int row;
	int column;

	gtk_hlist_get_selection_info(GTK_HLIST(widget),
				     event->x, event->y, &row, &column);
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		struct hx_user *user;

		user = gtk_hlist_get_row_data(GTK_HLIST(widget), row);
		if (user)
			users_open_message(ghtlc, user->uid, user->name);
		return 1;
	} else if (event->button == 3) {
		struct gchat *gchat;

		gchat = gchat_with_users_list(ghtlc, widget);
		if (!user_menu)
			user_menu = context_menu_new(user_menu_entries,
						     CM_NENTRIES(user_menu_entries));
		if (!(gchat->chat && gchat->chat->cid)) {
			if (invite_menu)
				context_menu_delete(invite_menu);
			invite_menu = invite_menu_new(ghtlc);
			context_menu_set_submenu(user_menu, 3, invite_menu);
		} else {
			context_menu_set_submenu(user_menu, 3, 0);
		}
		if (user_menu->entries[0].data != gchat) {
			guint i;
			for (i = 0; i < user_menu->nentries; i++)
				context_menu_set_data(user_menu, i, gchat);
		}
		gtk_menu_popup(GTK_MENU(user_menu->menu), 0, 0, 0, 0,
			       event->button, event->time);
		return 1;
	}

	return 0;
}

static void
users_list_destroy (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct ghtlc_conn *ghtlc = gchat->ghtlc;

	if (!gchat->chat || !gchat->chat->cid) {
		ghtlc->user_msgbtn = 0;
		ghtlc->user_infobtn = 0;
		ghtlc->user_kickbtn = 0;
		ghtlc->user_banbtn = 0;
		ghtlc->user_chatbtn = 0;
	}
	gchat->users_list = 0;
	gchat->users_vbox = 0;
}

void
create_users_window (struct ghtlc_conn *ghtlc, struct gchat *gchat)
{
	struct ghx_window *gwin;
	struct window_geometry *wg = 0;
	GtkWidget *window = 0;
	GtkWidget *users_window_scroll;
	GtkWidget *vbox;
	GtkWidget *hbuttonbox, *topframe;
	GtkWidget *msgbtn, *infobtn, *kickbtn, *banbtn, *chatbtn;
	GtkWidget *users_list;
	GtkTooltips *tooltips;

	if (!gchat->chat || !gchat->chat->cid) {
		gwin = window_create(ghtlc, WG_USERS);
		wg = gwin->wg;
		window = gwin->widget;
		changetitle(ghtlc, window, "Users");
	}

	users_list = gtk_hlist_new(1);
	GTK_HLIST(users_list)->want_stipple = 1;
	gtk_hlist_set_selection_mode(GTK_HLIST(users_list), GTK_SELECTION_EXTENDED);
	gtk_hlist_set_column_width(GTK_HLIST(users_list), 0, 240);
	gtk_hlist_set_row_height(GTK_HLIST(users_list), 18);
	gtk_hlist_set_shadow_type(GTK_HLIST(users_list), GTK_SHADOW_NONE);
	gtk_hlist_set_column_justification(GTK_HLIST(users_list), 0, GTK_JUSTIFY_LEFT);
	gtk_signal_connect(GTK_OBJECT(users_list), "button_press_event",
			   GTK_SIGNAL_FUNC(user_clicked), ghtlc);
	gtk_signal_connect_object(GTK_OBJECT(users_list), "destroy",
				  GTK_SIGNAL_FUNC(users_list_destroy), (gpointer)gchat);

	if (!ghtlc->users_style) {
		if (!ghtlc->users_font)
			ghtlc->users_font = gdk_fontset_load(def_users_font);
		if (ghtlc->users_font) {
			ghtlc->users_style = gtk_style_copy(gtk_widget_get_style(users_list));
			ghtlc->users_style->font = ghtlc->users_font;
		}
	}
	if (ghtlc->users_style)
		gtk_widget_set_style(users_list, ghtlc->users_style);

	users_window_scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(users_window_scroll) = 0;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(users_window_scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(users_window_scroll, 232, 232);
	gtk_container_add(GTK_CONTAINER(users_window_scroll), users_list);

	tooltips = gtk_tooltips_new();
	msgbtn = icon_button_new(ICON_MSG, "Message", users_list, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(msgbtn), "clicked",
				  GTK_SIGNAL_FUNC(user_message_btn), (gpointer)gchat);
	infobtn = icon_button_new(ICON_INFO, "Get Info", users_list, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(infobtn), "clicked",
				  GTK_SIGNAL_FUNC(user_info_btn), (gpointer)gchat);
	kickbtn = icon_button_new(ICON_KICK, "Kick", users_list, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(kickbtn), "clicked",
				  GTK_SIGNAL_FUNC(user_kick_btn), (gpointer)gchat);
	banbtn = icon_button_new(ICON_BAN, "Ban", users_list, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(banbtn), "clicked",
				  GTK_SIGNAL_FUNC(user_ban_btn), (gpointer)gchat);
	chatbtn = icon_button_new(ICON_CHAT, "Chat", users_list, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(chatbtn), "clicked",
				  GTK_SIGNAL_FUNC(user_chat_btn), (gpointer)gchat);

	vbox = gtk_vbox_new(0, 0);
	if (!gchat->chat || !gchat->chat->cid)
		gtk_widget_set_usize(vbox, wg->width - 24, wg->height);
	else
		gtk_widget_set_usize(vbox, 258, 258);

	topframe = gtk_frame_new(0);
	gtk_box_pack_start(GTK_BOX(vbox), topframe, 0, 0, 0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);

	hbuttonbox = gtk_hbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), chatbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), msgbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), infobtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), kickbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), banbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), users_window_scroll, 1, 1, 0);

	gchat->users_list = users_list;
	gchat->users_vbox = vbox;
	if (!gchat->chat || !gchat->chat->cid) {
		ghtlc->user_msgbtn = msgbtn;
		ghtlc->user_infobtn = infobtn;
		ghtlc->user_kickbtn = kickbtn;
		ghtlc->user_banbtn = banbtn;
		ghtlc->user_chatbtn = chatbtn;

		gtk_container_add(GTK_CONTAINER(window), vbox);
		gtk_widget_show_all(window);
	}

	gtk_widget_set_sensitive(msgbtn, ghtlc->connected);
	gtk_widget_set_sensitive(infobtn, ghtlc->connected);
	gtk_widget_set_sensitive(kickbtn, ghtlc->connected);
	gtk_widget_set_sensitive(banbtn, ghtlc->connected);
	gtk_widget_set_sensitive(chatbtn, ghtlc->connected);
}

void user_list (struct htlc_conn *htlc, struct hx_chat *chat);

void
open_users (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gchat *gchat;

	gchat = gchat_with_cid(ghtlc, 0);
	if (gchat->users_list) {
		if (gchat->gwin) {
			ghx_window_present(gchat->gwin);
		}
		return;
	}

	create_users_window(ghtlc, gchat);

	if (gchat->chat)
		user_list(ghtlc->htlc, gchat->chat);
}

void
user_list (struct htlc_conn *htlc, struct hx_chat *chat)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	struct hx_user *user;
	GtkWidget *users_list;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!chat->cid) {
		gchat = gchat_with_cid(ghtlc, 0);
		gchat->chat = chat;
		if (!gchat->users_list)
			open_users(ghtlc);
	} else {
		gchat = gchat_with_chat(ghtlc, chat);
		if (!gchat) {
			create_chat_window(ghtlc, chat);
			gchat = gchat_with_chat(ghtlc, chat);
		}
	}
	users_list = gchat->users_list;
	if (!users_list)
		return;

	gtk_hlist_freeze(GTK_HLIST(users_list));
	gtk_hlist_clear(GTK_HLIST(users_list));
	for (user = chat->user_list->next; user; user = user->next)
		hx_output.user_create(htlc, chat, user, user->name, user->icon, user->color);
	gtk_hlist_thaw(GTK_HLIST(users_list));
}

void
users_clear (struct htlc_conn *htlc, struct hx_chat *chat)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_chat(ghtlc, chat);
	if (!gchat || !gchat->users_list)
		return;
	gtk_hlist_clear(GTK_HLIST(gchat->users_list));
}

void
user_create (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
	     const char *nam, u_int16_t icon, u_int16_t color)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *users_list;
	gint row;
	gchar *nulls[1] = {0};

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!chat->cid) {
		gchat = gchat_with_cid(ghtlc, 0);
		gchat->chat = chat;
		if (!gchat->users_list)
			open_users(ghtlc);
	} else {
		gchat = gchat_with_chat(ghtlc, chat);
		if (!gchat) {
			create_chat_window(ghtlc, chat);
			gchat = gchat_with_chat(ghtlc, chat);
		}
	}
	users_list = gchat->users_list;
	if (!users_list)
		return;

	row = gtk_hlist_append(GTK_HLIST(users_list), nulls);
	gtk_hlist_set_row_data(GTK_HLIST(users_list), row, user);
	gtk_hlist_set_foreground(GTK_HLIST(users_list), row, colorgdk(color));
	if (color & 1)
		pixc = load_icon(users_list, icon, &user_icon_files, 0, 1);
	else
		pixc = load_icon(users_list, icon, &user_icon_files, 0, 0);
	if (pixc) {
		pixmap = pixc->pixmap;
		mask = pixc->mask;
	} else {
		pixmap = 0;
		mask = 0;
	}
#if defined(CONFIG_ICONV)
	{
		size_t out_len;
		char *out_p;
		char dnam[256];

		out_len = convbuf(g_encoding, g_server_encoding, nam, strlen(nam), &out_p);
		if (out_len) {
			if (out_len >= sizeof(dnam))
				out_len = sizeof(dnam)-1;
			memcpy(dnam, out_p, out_len);
			dnam[out_len] = 0;
			if (!pixmap) 
				gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, dnam);
			else 
				gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, dnam, 34, pixmap, mask);
			xfree(out_p);
		} else {
			if (!pixmap) 
				gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, nam);
			else 
				gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, nam, 34, pixmap, mask);
		}
	}
#else
	if (!pixmap) 
		gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, nam);
	else 
		gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, nam, 34, pixmap, mask);
#endif
}

void
user_delete (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	GtkWidget *users_list;
	gint row;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_chat(ghtlc, chat);
	if (!gchat)
		return;
	users_list = gchat->users_list;
	if (!users_list)
		return;

	row = gtk_hlist_find_row_from_data(GTK_HLIST(users_list), user);
	gtk_hlist_remove(GTK_HLIST(users_list), row);
}

void
user_change (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
	     const char *nam, u_int16_t icon, u_int16_t color)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *users_list;
	gint row;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_chat(ghtlc, chat);
	if (!gchat)
		return;
	users_list = gchat->users_list;
	if (!chat->cid) {
		for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
			if (gchat->chat && gchat->chat->cid) {
				struct hx_user *u;

				u = hx_user_with_uid(gchat->chat->user_list, user->uid);
				if (u)
					user_change(htlc, gchat->chat, u, nam, icon, color);
			}
		}
	}
	if (!users_list)
		return;

	row = gtk_hlist_find_row_from_data(GTK_HLIST(users_list), user);
	gtk_hlist_set_row_data(GTK_HLIST(users_list), row, user);
	gtk_hlist_set_foreground(GTK_HLIST(users_list), row, colorgdk(color));
	if (color & 1)
		pixc = load_icon(users_list, icon, &user_icon_files, 0, 1);
	else 
		pixc = load_icon(users_list, icon, &user_icon_files, 0, 0);

	if (pixc) {
		pixmap = pixc->pixmap;
		mask = pixc->mask;
	} else {
		pixmap = 0;
		mask = 0;
	}
#if defined(CONFIG_ICONV)
	{
		size_t out_len;
		char *out_p;
		char dnam[256];

		out_len = convbuf(g_encoding, g_server_encoding, nam, strlen(nam), &out_p);
		if (out_len) {
			if (out_len >= sizeof(dnam))
				out_len = sizeof(dnam)-1;
			memcpy(dnam, out_p, out_len);
			dnam[out_len] = 0;
			if (!pixmap) 
				gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, dnam);
			else 
				gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, dnam, 34, pixmap, mask);
			xfree(out_p);
		} else {
			if (!pixmap) 
				gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, nam);
			else 
				gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, nam, 34, pixmap, mask);
		}
	}
#else
	if (!pixmap) 
		gtk_hlist_set_text(GTK_HLIST(users_list), row, 0, nam);
	else 
		gtk_hlist_set_pixtext(GTK_HLIST(users_list), row, 0, nam, 34, pixmap, mask);
#endif
}

static void
sendmessage (gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	struct ghtlc_conn *ghtlc = mc->ghtlc;
	GtkWidget *msgtext;
	GtkWidget *msgwin;
	u_int32_t uid;
	char *msgbuf;

	msgtext = mc->to_text;
	msgwin = mc->win;
	uid = mc->uid;
	msgbuf = gtk_editable_get_chars(GTK_EDITABLE(msgtext), 0, -1);
	hx_send_msg(ghtlc->htlc, uid, msgbuf, strlen(msgbuf), 0);
	g_free(msgbuf);

	gtk_widget_destroy(msgwin);
}

static void
replymessage (gpointer data)
{
	struct msgchat *mc = (struct msgchat *)data;
	GtkWidget *msgtext;
	GtkWidget *vscrollbar;
	GtkWidget *msgwin;
	GtkWidget *vpane;
	GtkWidget *thbox;
	GtkWidget *btn;

	msgwin = mc->win;
	vpane = mc->vpane;
	btn = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(msgwin), "btn");

	msgtext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(msgtext), 1);
	mc->to_text = msgtext;
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(msgtext)->vadj);
	thbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(thbox), msgtext, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(thbox), vscrollbar, 0, 0, 0);
	gtk_paned_add2(GTK_PANED(vpane), thbox);
	mc->to_hbox = thbox;
	gtk_widget_set_usize(mc->from_hbox, 300, 150);
	gtk_widget_set_usize(mc->to_hbox, 300, 150);
	gtk_widget_show_all(thbox);

	gtk_label_set_text(GTK_LABEL(GTK_BIN(btn)->child), "Send and Close");
	gtk_widget_set_usize(btn, 110, -2);
	gtk_signal_disconnect_by_func(GTK_OBJECT(btn), GTK_SIGNAL_FUNC(replymessage), mc);
	gtk_signal_connect_object(GTK_OBJECT(btn), "clicked",
				  GTK_SIGNAL_FUNC(sendmessage), (gpointer)mc);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(btn), "clicked");
	if (mc->chat) {
		gtk_signal_connect(GTK_OBJECT(msgtext), "key_press_event",
				   GTK_SIGNAL_FUNC(msgchat_input_key_press), mc);
		gtk_signal_connect(GTK_OBJECT(msgtext), "activate",
				   GTK_SIGNAL_FUNC(msgchat_input_activate), mc);
		gtk_widget_set_usize(mc->from_hbox, 300, 180);
		gtk_widget_set_usize(mc->to_hbox, 300, 40);
	}
}

void
output_msg (struct htlc_conn *htlc, u_int32_t uid, const char *nam, const char *msgbuf, u_int16_t msglen)
{
	GtkWidget *msgwin;
	GtkWidget *thbox;
	GtkWidget *vbox1;
	GtkWidget *hbox2;
	GtkWidget *infobtn;
	GtkWidget *chatbtn;
	GtkWidget *msgtext;
	GtkWidget *vscrollbar;
	GtkWidget *hbox1;
	GtkWidget *chat_btn;
	GtkWidget *replybtn;
	GtkWidget *okbtn;
	GtkWidget *pixmap;
	GtkWidget *vpane;
	GtkTooltips *tooltips;
	struct ghtlc_conn *ghtlc;
	char title[64];
	struct hx_chat *chat;
	struct hx_user *user;
	u_int16_t icon;
	struct msgchat *mc;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	mc = msgchat_with_uid(ghtlc, uid);
	if (mc) {
		guint len;

		msgtext = mc->from_text;
		len = gtk_text_get_length(GTK_TEXT(msgtext));
		gtk_text_set_point(GTK_TEXT(msgtext), len);
		if (len)
			gtk_text_insert(GTK_TEXT(msgtext), 0, MSG_FROM_COLOR, 0, "\n", 1);
		gtk_text_insert(GTK_TEXT(msgtext), 0, MSG_FROM_COLOR, 0, msgbuf, msglen);
		return;
	}

	if (!uid)
		nam = "Server";

	snprintf(title, sizeof(title), "%s (%u)", nam, uid);

	msgwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(msgwin), 1, 1, 0);
	gtk_window_set_title(GTK_WINDOW(msgwin), "msgwin");
	gtk_window_set_default_size(GTK_WINDOW(msgwin), 300, 300);
	gtk_window_set_title(GTK_WINDOW(msgwin), title);

	mc = msgchat_new(ghtlc, uid);
	mc->ghtlc = ghtlc;
	mc->win = msgwin;
	strcpy(mc->name, nam);
	gtk_signal_connect_object(GTK_OBJECT(msgwin), "destroy",
				  GTK_SIGNAL_FUNC(destroy_msgchat), (gpointer)mc);

	vbox1 = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(msgwin), vbox1);

	hbox2 = gtk_hbox_new(0, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox2, 0, 0, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox2), 5);

	tooltips = gtk_tooltips_new();

	infobtn = icon_button_new(ICON_INFO, "Get Info", msgwin, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(infobtn), "clicked",
				  GTK_SIGNAL_FUNC(msgwin_get_info), (gpointer)mc);
	gtk_box_pack_start(GTK_BOX(hbox2), infobtn, 0, 0, 0);

	chatbtn = icon_button_new(ICON_CHAT, "Chat", msgwin, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(chatbtn), "clicked",
				  GTK_SIGNAL_FUNC(msgwin_chat), (gpointer)mc);
	gtk_box_pack_start(GTK_BOX(hbox2), chatbtn, 0, 0, 0);

	chat = hx_chat_with_cid(ghtlc->htlc, 0);
	if (chat) {
		user = hx_user_with_uid(chat->user_list, uid);
		if (user) {
			icon = user->icon;
			gtk_widget_realize(msgwin);
			pixmap = user_pixmap(msgwin, ghtlc->users_font, colorgdk(user->color), icon, nam);
			if (pixmap)
				gtk_box_pack_start(GTK_BOX(hbox2), pixmap, 0, 1, 0);				
		}
	}

	msgtext = gtk_text_new(0, 0);
	mc->from_text = msgtext;
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(msgtext)->vadj);
	thbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(thbox), msgtext, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(thbox), vscrollbar, 0, 0, 0);

	vpane = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(vpane), thbox);
	gtk_box_pack_start(GTK_BOX(vbox1), vpane, 1, 1, 0);
	mc->vpane = vpane;

	mc->from_hbox = thbox;
	gtk_widget_set_usize(mc->from_hbox, 300, 220);

	hbox1 = gtk_hbox_new(0, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, 0, 0, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);

	chat_btn = gtk_check_button_new_with_label("Chat");
	gtk_signal_connect(GTK_OBJECT(chat_btn), "clicked",
			   GTK_SIGNAL_FUNC(chat_chk_activate), mc);
	gtk_box_pack_start(GTK_BOX(hbox1), chat_btn, 0, 0, 0);
	gtk_widget_set_usize(chat_btn, 55, -2);
	mc->chat_chk = chat_btn;

	replybtn = gtk_button_new_with_label("Reply");
	gtk_box_pack_end(GTK_BOX(hbox1), replybtn, 0, 0, 0);
	gtk_widget_set_usize(replybtn, 55, -2);
	gtk_object_set_data(GTK_OBJECT(msgwin), "btn", replybtn);
	gtk_signal_connect_object(GTK_OBJECT(replybtn), "clicked",
				  GTK_SIGNAL_FUNC(replymessage), (gpointer)mc);

	okbtn = gtk_button_new_with_label("Dismiss");
	gtk_box_pack_end(GTK_BOX(hbox1), okbtn, 0, 0, 0);
	gtk_widget_set_usize(okbtn, 55, -2);
	gtk_signal_connect_object(GTK_OBJECT(okbtn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)msgwin);

	gtk_text_insert(GTK_TEXT(msgtext), 0, 0, 0, msgbuf, msglen);

	gtk_widget_show_all(msgwin);

	keyaccel_attach(ghtlc, msgwin);
}
