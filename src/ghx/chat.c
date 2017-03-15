#include "hx.h"
#include "hx_gtk.h"
#include <gdk/gdkkeysyms.h>
#include "history.h"
#include "xmalloc.h"
#include <string.h>

extern GdkColor colors[16];
extern GdkColor gdk_user_colors[4];
extern struct ghtlc_conn *ghtlc_conn_with_htlc (struct htlc_conn *htlc);
extern struct ghx_window *window_create (struct ghtlc_conn *ghtlc, unsigned int wgi);
extern void create_users_window (struct ghtlc_conn *ghtlc, struct gchat *gchat);

struct gchat *
gchat_new (struct ghtlc_conn *ghtlc, struct hx_chat *chat)
{
	struct gchat *gchat;

	gchat = xmalloc(sizeof(struct gchat));
	memset(gchat, 0, sizeof(struct gchat));
	gchat->next = 0;
	gchat->prev = ghtlc->gchat_list;
	if (ghtlc->gchat_list)
		ghtlc->gchat_list->next = gchat;
	ghtlc->gchat_list = gchat;
	gchat->ghtlc = ghtlc;
	gchat->chat = chat;

	return gchat;
}

static void
gchat_delete (struct ghtlc_conn *ghtlc, struct gchat *gchat)
{
	if (gchat->gwin) {
		GtkWidget *widget = gchat->gwin->widget;
		gchat->gwin = 0;
		gtk_widget_destroy(widget);
	}
	if (gchat->next)
		gchat->next->prev = gchat->prev;
	if (gchat->prev)
		gchat->prev->next = gchat->next;
	if (gchat == ghtlc->gchat_list)
		ghtlc->gchat_list = gchat->prev;
	xfree(gchat);
}

struct gchat *
gchat_with_widget (struct ghtlc_conn *ghtlc, GtkWidget *widget)
{
	struct gchat *gchat;

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (!gchat->gwin)
			continue;
		if (gchat->gwin->widget == widget)
			return gchat;
	}

	return 0;
}

struct gchat *
gchat_with_users_list (struct ghtlc_conn *ghtlc, GtkWidget *users_list)
{
	struct gchat *gchat;

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (gchat->users_list == users_list)
			return gchat;
	}

	return 0;
}

struct gchat *
gchat_with_chat (struct ghtlc_conn *ghtlc, struct hx_chat *chat)
{
	struct gchat *gchat;

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if (gchat->chat == chat)
			return gchat;
	}

	return 0;
}

struct gchat *
gchat_with_cid (struct ghtlc_conn *ghtlc, u_int32_t cid)
{
	struct gchat *gchat;

	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		if ((cid == 0 && !gchat->chat) || gchat->chat->cid == cid)
			return gchat;
	}

	return 0;
}

static void
chat_input_activate (GtkWidget *widget, gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	GtkText *text;
	guint point, len;
	gchar *chars;

	text = GTK_TEXT(widget);
	point = gtk_text_get_point(text);
	len = gtk_text_get_length(text);
	chars = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
	add_history(gchat->chat_history, chars);
	using_history(gchat->chat_history);
	LF2CR(chars, len);
	hotline_client_input(gchat->ghtlc->htlc, gchat->chat, chars);
	g_free(chars);

	gtk_text_set_point(text, 0);
	gtk_text_forward_delete(text, len);
	gtk_text_set_editable(text, 1);
}

static void
chat_input_changed (GtkWidget *widget, gpointer data)
{
	guint point = GPOINTER_TO_INT(data);
	guint len = gtk_text_get_length(GTK_TEXT(widget));

	if (!point)
		point = len;
	gtk_text_set_point(GTK_TEXT(widget), point);
	gtk_editable_set_position(GTK_EDITABLE(widget), point);

	gtk_signal_disconnect_by_func(GTK_OBJECT(widget), chat_input_changed, data);
}

/*
 * sniff the keys coming to the text widget
 * this is called before the text's key_press function is called
 * gtk will emit an activate signal on return only when control
 * is held or the text is not editable.
 */
static gint
chat_input_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	GtkText *text;
	char *buf = 0, *p;
	char *line = 0;
	int linepoint = 0;
	guint point, len;
	guint k;
	HIST_ENTRY *hent;

	k = event->keyval;
	text = GTK_TEXT(widget);
	point = gtk_editable_get_position(GTK_EDITABLE(text));
	len = gtk_text_get_length(text);
	if (k == GDK_Return) {
		if (len == point)
			gtk_text_set_editable(text, 0);
		return 1;
	} else if (k == GDK_Tab) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
		if ((event->state & GDK_CONTROL_MASK)) {
			gtk_container_focus(GTK_CONTAINER(gchat->gwin->widget), GTK_DIR_TAB_BACKWARD);
			return 1;
		}
		buf = xmalloc(len + 4096);
		p = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
		strcpy(buf, p);
		g_free(p);
		linepoint = hotline_client_tab(gchat->ghtlc->htlc, gchat->chat, buf, point);
		line = buf;
	} else if (len == point) {
		if (k == GDK_Up) {
			hent = previous_history(gchat->chat_history);
			if (hent)
				line = hent->line;
		} else if (k == GDK_Down) {
			hent = next_history(gchat->chat_history);
			if (hent)
				line = hent->line;
		}
	}
	if (line) {
		GdkColor *fgcolor = &widget->style->fg[GTK_STATE_NORMAL];
		GdkColor *bgcolor = &widget->style->bg[GTK_STATE_NORMAL];

		gtk_text_freeze(text);
		gtk_text_set_point(text, 0);
		len = gtk_text_get_length(text);
		gtk_text_forward_delete(text, len);
		len = strlen(line);
		gtk_text_set_point(text, 0);
		gtk_text_insert(text, 0, fgcolor, bgcolor, line, len);
		gtk_text_thaw(text);
		gtk_signal_connect(GTK_OBJECT(text), "changed",
				   GTK_SIGNAL_FUNC(chat_input_changed), GINT_TO_POINTER(linepoint));
		gtk_editable_changed(GTK_EDITABLE(text));
		if (line == buf)
			xfree(buf);
		return 1;
	}

	return 0;
}

static void
init_colors (GtkWidget *widget)
{
	GdkColormap *colormap;
	int i;

	colors[0].red = 0x0000;
	colors[0].green = 00000;
	colors[0].blue = 0x0000;
	colors[1].red = 0xa000;
	colors[1].green = 0x0000;
	colors[1].blue = 0x0000;
	colors[2].red = 0x0000;
	colors[2].green = 0xa000;
	colors[2].blue = 0x0000;
	colors[3].red = 0xa000;
	colors[3].green = 0xa000;
	colors[3].blue = 0x0000;
	colors[4].red = 0x0000;
	colors[4].green = 0x0000;
	colors[4].blue = 0xa000;
	colors[5].red = 0xa000;
	colors[5].green = 0x0000;
	colors[5].blue = 0xa000;
	colors[6].red = 0x0000;
	colors[6].green = 0xa000;
	colors[6].blue = 0xa000;
	colors[7].red = 0xa000;
	colors[7].green = 0xa000;
	colors[7].blue = 0xa000;
	colors[8].red = 0x0000;
	colors[8].green = 00000;
	colors[8].blue = 0x0000;
	colors[9].red = 0xffff;
	colors[9].green = 0x0000;
	colors[9].blue = 0x0000;
	colors[10].red = 0x0000;
	colors[10].green = 0xffff;
	colors[10].blue = 0x0000;
	colors[11].red = 0xffff;
	colors[11].green = 0xffff;
	colors[11].blue = 0x0000;
	colors[12].red = 0x0000;
	colors[12].green = 0x0000;
	colors[12].blue = 0xffff;
	colors[13].red = 0xffff;
	colors[13].green = 0x0000;
	colors[13].blue = 0xffff;
	colors[14].red = 0x0000;
	colors[14].green = 0xffff;
	colors[14].blue = 0xffff;
	colors[15].red = 0xffff;
	colors[15].green = 0xffff;
	colors[15].blue = 0xffff;

	colormap = gtk_widget_get_colormap(widget);
	for (i = 0; i < 16; i++) {
		colors[i].pixel = (gulong)(((colors[i].red & 0xff00) << 8)
				+ (colors[i].green & 0xff00)
				+ ((colors[i].blue & 0xff00) >> 8));
		if (!gdk_colormap_alloc_color(colormap, &colors[i], 0, 1))
			hxd_log("alloc color failed");
	}

	gdk_user_colors[0].red = 0;
	gdk_user_colors[0].green = 0;
	gdk_user_colors[0].blue = 0;
	gdk_user_colors[1].red = 0xa0a0;
	gdk_user_colors[1].green = 0xa0a0;
	gdk_user_colors[1].blue = 0xa0a0;
	gdk_user_colors[2].red = 0xffff;
	gdk_user_colors[2].green = 0;
	gdk_user_colors[2].blue = 0;
	gdk_user_colors[3].red = 0xffff;
	gdk_user_colors[3].green = 0xa7a7;
	gdk_user_colors[3].blue = 0xb0b0;
	for (i = 0; i < 4; i++) {
		gdk_user_colors[i].pixel = (gulong)(((gdk_user_colors[i].red & 0xff00) << 8)
				+ (gdk_user_colors[i].green & 0xff00)
				+ ((gdk_user_colors[i].blue & 0xff00) >> 8));
		if (!gdk_colormap_alloc_color(colormap, &gdk_user_colors[i], 0, 1))
			hxd_log("alloc color failed");
	}
}

static void
create_chat_text (struct ghtlc_conn *ghtlc, GtkWidget **intextp, GtkWidget **outtextp, GtkWidget **vscrollbarp,
		  GtkStyle **stylep, int first)
{
	GtkWidget *intext, *outtext;
	GtkWidget *vscrollbar;
	GtkStyle *style;
	GdkFont *font;

	outtext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(outtext), 0);
	gtk_text_set_word_wrap(GTK_TEXT(outtext), 1);

	intext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(intext), 1);
	gtk_text_set_word_wrap(GTK_TEXT(intext), 1);

	if (first)
		init_colors(outtext);

	style = gtk_style_new();
	style->base[GTK_STATE_NORMAL] = colors[8];
	style->bg[GTK_STATE_NORMAL] = colors[8];
	style->fg[GTK_STATE_NORMAL] = colors[15];
	style->text[GTK_STATE_NORMAL] = colors[15];
	style->light[GTK_STATE_NORMAL] = colors[15];
	style->dark[GTK_STATE_NORMAL] = colors[8];
	font = ghtlc->chat_font;
	if (font)
		gdk_font_ref(font);
	else {
		font = gdk_fontset_load(def_font);
		if (!font)
			font = gdk_fontset_load("fixed");
	}
	gdk_font_unref(style->font);
	style->font = font;
	gtk_widget_set_style(intext, style);
	gtk_widget_set_style(outtext, style);

	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(outtext)->vadj);

	*intextp = intext;
	*outtextp = outtext;
	*vscrollbarp = vscrollbar;
	*stylep = style;
}

static void
set_subject (GtkWidget *widget, gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	char *subject;

	subject = gtk_entry_get_text(GTK_ENTRY(widget));
	if (gchat && gchat->chat)
		hx_set_subject(gchat->ghtlc->htlc, gchat->chat->cid, subject);
}

void
gchat_create_chat_text (struct gchat *gchat, int first)
{
	GtkWidget *intext, *outtext;
	GtkWidget *vscrollbar;
	GtkWidget *subject_entry;
	GtkStyle *style;

	create_chat_text(gchat->ghtlc, &intext, &outtext, &vscrollbar, &style, first);
	gchat->chat_input_text = intext;
	gchat->chat_output_text = outtext;
	gchat->chat_vscrollbar = vscrollbar;

	subject_entry = gtk_entry_new();
	gtk_widget_set_style(subject_entry, style);
	gchat->subject_entry = subject_entry;
	gtk_signal_connect(GTK_OBJECT(subject_entry), "activate",
			   GTK_SIGNAL_FUNC(set_subject), gchat);

	gtk_signal_connect(GTK_OBJECT(intext), "key_press_event",
			   GTK_SIGNAL_FUNC(chat_input_key_press), gchat);
	gtk_signal_connect(GTK_OBJECT(intext), "activate",
			   GTK_SIGNAL_FUNC(chat_input_activate), gchat);

	if (!(gchat->chat && gchat->chat->cid)) {
		gtk_object_ref(GTK_OBJECT(intext));
		gtk_object_sink(GTK_OBJECT(intext));
		gtk_object_ref(GTK_OBJECT(outtext));
		gtk_object_sink(GTK_OBJECT(outtext));
		gtk_object_ref(GTK_OBJECT(vscrollbar));
		gtk_object_sink(GTK_OBJECT(vscrollbar));
		gtk_object_ref(GTK_OBJECT(subject_entry));
		gtk_object_sink(GTK_OBJECT(subject_entry));
	}

	gchat->chat_history = history_new();
	stifle_history(gchat->chat_history, hist_size);
}

gint
chat_delete_event (gpointer data)
{
	struct gchat *gchat = (struct gchat *)data;
	struct ghx_window *gwin;

	if (!gchat)
		return 1;
	gwin = gchat->gwin;
	if (!gwin)
		return 1;
	if (gchat->chat && gchat->chat->cid) {
		hx_chat_part(gchat->ghtlc->htlc, gchat->chat);
		return 1;
	} else {
		gtk_container_remove(GTK_CONTAINER(gchat->chat_in_hbox), gchat->chat_input_text);
		gtk_container_remove(GTK_CONTAINER(gchat->chat_out_hbox), gchat->chat_output_text);
		gtk_container_remove(GTK_CONTAINER(gchat->chat_out_hbox), gchat->chat_vscrollbar);
		gtk_container_remove(GTK_CONTAINER(gchat->subject_hbox), gchat->subject_entry);
		gchat->gwin = 0;
	}

	return 0;
}

static GtkWidget *
create_chat_widget (struct ghtlc_conn *ghtlc, struct gchat *gchat,
		    struct window_geometry *wg)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *intext, *outtext;
	GtkWidget *vscrollbar;
	GtkWidget *outputframe, *subjectframe, *inputframe;
	GtkWidget *subject_entry;
	GtkWidget *vpane, *hpane;
	GtkStyle *style;

	vbox = gtk_vbox_new(0, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

	outputframe = gtk_frame_new(0);
	gtk_frame_set_shadow_type(GTK_FRAME(outputframe), GTK_SHADOW_IN);
	inputframe = gtk_frame_new(0);
	gtk_frame_set_shadow_type(GTK_FRAME(inputframe), GTK_SHADOW_IN);

	if (gchat) {
		outtext = gchat->chat_output_text;
		if (!outtext) {
			gchat_create_chat_text(gchat, 0);
			outtext = gchat->chat_output_text;
		}
		vscrollbar = gchat->chat_vscrollbar;
		intext = gchat->chat_input_text;
		style = gtk_widget_get_style(outtext);
	} else {
		create_chat_text(ghtlc, &intext, &outtext, &vscrollbar, &style, 0);
	}

	if (gchat) {
		hbox = gtk_hbox_new(0, 0);
		gchat->subject_hbox = hbox;
		gtk_widget_set_usize(hbox, (wg->width<<6)/82, 20);
		subjectframe = gtk_frame_new(0);
		gtk_frame_set_shadow_type(GTK_FRAME(subjectframe), GTK_SHADOW_OUT);
		gtk_container_add(GTK_CONTAINER(subjectframe), hbox);
		subject_entry = gchat->subject_entry;
		gtk_box_pack_start(GTK_BOX(hbox), subject_entry, 1, 1, 0);
		if (gchat->chat)
			gtk_entry_set_text(GTK_ENTRY(subject_entry), gchat->chat->subject);
		gtk_box_pack_start(GTK_BOX(vbox), subjectframe, 0, 1, 0);
	}

	vpane = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(vpane), outputframe);
	gtk_paned_add2(GTK_PANED(vpane), inputframe);

	gtk_box_pack_start(GTK_BOX(vbox), vpane, 1, 1, 0); 

	hbox = gtk_hbox_new(0, 0);
	gtk_widget_set_usize(hbox, (wg->width<<6)/82, (wg->height<<6)/100);
	gtk_container_add(GTK_CONTAINER(outputframe), hbox);
	gtk_widget_set_usize(outputframe, (wg->width<<6)/82, (wg->height<<6)/100);

	if (gchat)
		gchat->chat_out_hbox = hbox;
	gtk_box_pack_start(GTK_BOX(hbox), outtext, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, 0, 0, 0);

	hbox = gtk_hbox_new(0, 0);
	gtk_widget_set_usize(hbox, (wg->width<<6)/100, 50);
	gtk_container_add(GTK_CONTAINER(inputframe), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), intext, 1, 1, 0);
	gchat->chat_in_hbox = hbox;

	gtk_widget_set_usize(vbox, wg->width-4, wg->height-4);
	if (gchat && gchat->chat && gchat->chat->cid) {
		create_users_window(ghtlc, gchat);
		gtk_widget_set_usize(gchat->users_vbox, 200, wg->height-4);
		hpane = gtk_hpaned_new();
		gtk_paned_add1(GTK_PANED(hpane), vbox);
		gtk_paned_add2(GTK_PANED(hpane), gchat->users_vbox);
		return hpane;
	} else {
		return vbox;
	}
}

void
create_chat_window (struct ghtlc_conn *ghtlc, struct hx_chat *chat)
{
	struct gchat *gchat;
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *container;
	GtkWidget *window;
	char title[32];

	gchat = gchat_with_chat(ghtlc, chat);
	if (gchat && gchat->gwin)
		return;
	gwin = window_create(ghtlc, WG_CHAT);
	wg = gwin->wg;
	window = gwin->widget;
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	if (chat && chat->cid)
		gtk_widget_set_usize(window, wg->width+200, wg->height);
	else
		gtk_widget_set_usize(window, wg->width, wg->height);
	if (!gchat)
		gchat = gchat_new(ghtlc, chat);
	gchat->gwin = gwin;
	gtk_signal_connect_object(GTK_OBJECT(window), "delete_event",
				  GTK_SIGNAL_FUNC(chat_delete_event), (gpointer)gchat);

	if (chat && chat->cid)
		sprintf(title, "Chat 0x%x", chat->cid);
	else
		strcpy(title, "Chat");
	changetitle(ghtlc, window, title);

	container = create_chat_widget(ghtlc, gchat, wg);
	gtk_container_add(GTK_CONTAINER(window), container);
	gtk_widget_show_all(window);
}

void
chat_subject (struct htlc_conn *htlc, u_int32_t cid, const char *subject)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_cid(ghtlc, cid);
	if (gchat)
		gtk_entry_set_text(GTK_ENTRY(gchat->subject_entry), subject);
}

void
chat_password (struct htlc_conn *htlc, u_int32_t cid, const u_int8_t *pass)
{
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "chat 0x%x password: %s\n", cid, pass);
}

static void
join_chat (GtkWidget *widget, gpointer data)
{
	struct htlc_conn *htlc = (struct htlc_conn *)data;
	u_int32_t cid = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(widget), "cid"));
	GtkWidget *window = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "dialog");

	gtk_widget_destroy(window);
	hx_chat_join(htlc, cid, 0, 0);
}

void
chat_invite (struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid, const char *name)
{
	GtkWidget *dialog;
	GtkWidget *join;
	GtkWidget *cancel;
	GtkWidget *hbox;
	GtkWidget *label;
	char message[64];

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), "Chat Invitation");
	gtk_container_border_width(GTK_CONTAINER(dialog), 4);
	snprintf(message, 64, "Invitation to chat 0x%x from %s (%u)", cid, name, uid);

	label = gtk_label_new(message);
	join = gtk_button_new_with_label("Join");
	gtk_object_set_data(GTK_OBJECT(join), "dialog", dialog);
	gtk_object_set_data(GTK_OBJECT(join), "cid", GINT_TO_POINTER(cid));
	gtk_signal_connect(GTK_OBJECT(join), "clicked",
		GTK_SIGNAL_FUNC(join_chat), htlc);

	cancel = gtk_button_new_with_label("Decline");
	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
		GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(dialog));

	GTK_WIDGET_SET_FLAGS(join, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);

	hbox = gtk_hbox_new(0,0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), hbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), join, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cancel, 0, 0, 0);
	gtk_widget_grab_default(join);

	gtk_widget_show_all(dialog);
}

void
chat_delete (struct htlc_conn *htlc, struct hx_chat *chat)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_chat(ghtlc, chat);
	if (!gchat)
		return;
	if (chat->cid == 0)
		gchat->chat = 0;
	else
		gchat_delete(ghtlc, gchat);
}

void
open_chat (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct hx_chat *chat;

	for (chat = ghtlc->htlc->chat_list; chat; chat = chat->prev)
		if (!chat->prev)
			break;

	create_chat_window(ghtlc, chat);
}

