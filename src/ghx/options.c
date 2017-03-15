#include "hx.h"
#include "hx_gtk.h"
#include "gtk_hlist.h"
#include "xmalloc.h"
#include "icons.h"
#include <string.h>

#if defined(GTK_USES_X11)
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif

struct options_context {
	struct ghtlc_conn *ghtlc;
	struct ghx_window *gwin;
	GtkWidget *showjoin_btn;
	GtkWidget *showpart_btn;
	GtkWidget *showchange_btn;
	GtkWidget *nick_entry;
	GtkWidget *icon_entry;
	GtkWidget *icon_list;
	GtkWidget **font_entries;
	GtkWidget **userfont_entries;
	unsigned int nfonts;
	unsigned int nfound;
	u_int32_t icon_high;
};

static void
options_change (gpointer data)
{
	struct options_context *oc = (struct options_context *)data;
	struct ghtlc_conn *ghtlc = oc->ghtlc;
	char *fontstr, *fp;
	char *nicknam;
	char *iconstr;
	u_int16_t icon;
	unsigned int i, fplen, fslen;

	fontstr = 0;
	fslen = 0;
	for (i = 0; i < oc->nfonts; i++) {
		fp = gtk_entry_get_text(GTK_ENTRY(oc->font_entries[i]));
		if (fp && *fp) {
			fplen = strlen(fp);
			fontstr = xrealloc(fontstr, fslen+1+fplen+1);
			if (fslen)
				fontstr[fslen] = ',';
			memcpy(fontstr+fslen+(fslen?1:0), fp, fplen);
			fslen += fplen;
			fontstr[fslen] = 0;
		}
	}
	if (fontstr && *fontstr) {
		variable_set(ghtlc->htlc, 0, "chat_font[0][0]", fontstr);
		xfree(fontstr);
	}
	fontstr = 0;
	fslen = 0;
	for (i = 0; i < oc->nfonts; i++) {
		fp = gtk_entry_get_text(GTK_ENTRY(oc->userfont_entries[i]));
		if (fp && *fp) {
			fplen = strlen(fp);
			fontstr = xrealloc(fontstr, fslen+1+fplen+1);
			if (fslen)
				fontstr[fslen] = ',';
			memcpy(fontstr+fslen+(fslen?1:0), fp, fplen);
			fslen += fplen;
			fontstr[fslen] = 0;
		}
	}
	if (fontstr && *fontstr) {
		variable_set(ghtlc->htlc, 0, "users_font[0][0]", fontstr);
		xfree(fontstr);
	}
	variable_set(ghtlc->htlc, 0, "tty_show_user_joins",
		     GTK_TOGGLE_BUTTON(oc->showjoin_btn)->active ? "1" : "0");
	variable_set(ghtlc->htlc, 0, "tty_show_user_parts",
		     GTK_TOGGLE_BUTTON(oc->showpart_btn)->active ? "1" : "0");
	variable_set(ghtlc->htlc, 0, "tty_show_user_changes",
		     GTK_TOGGLE_BUTTON(oc->showchange_btn)->active ? "1" : "0");
	iconstr = gtk_entry_get_text(GTK_ENTRY(oc->icon_entry));
	nicknam = gtk_entry_get_text(GTK_ENTRY(oc->nick_entry));
	icon = strtoul(iconstr, 0, 0);

	hx_change_name_icon(ghtlc->htlc, nicknam, icon);

	xfree(oc->font_entries);
	gtk_widget_destroy(oc->gwin->widget);
}

static void
adjust_icon_list (struct options_context *oc, unsigned int height)
{
	GtkWidget *icon_list = oc->icon_list;
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	char *nam = "";
	gint row;
	gchar *text[2] = {0, 0};
	u_int32_t icon;
	unsigned int nfound = 0;
	char buf[16];

	text[1] = buf;
	height = height/18;
	for (icon = oc->icon_high; icon < 0x10000; icon++) {
		if (nfound >= height)
			break;
		pixc = load_icon(icon_list, icon, &user_icon_files, 1, 0);
		if (!pixc)
			continue;
		nfound++;
		pixmap = pixc->pixmap;
		mask = pixc->mask;
		sprintf(buf, "%u", icon);
		row = gtk_hlist_append(GTK_HLIST(icon_list), text);
		gtk_hlist_set_row_data(GTK_HLIST(icon_list), row, (gpointer)icon);
		gtk_hlist_set_pixtext(GTK_HLIST(icon_list), row, 0, nam, 34, pixmap, mask);
	}
	oc->icon_high = icon;
	oc->nfound += nfound;
}

static void
icon_list_scrolled (GtkAdjustment *adj, gpointer data)
{
	struct options_context *oc = (struct options_context *)data;

	adjust_icon_list(oc, (unsigned int)adj->page_size);
}

static void
icon_row_selected (GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer data)
{
	struct options_context *oc = (struct options_context *)data;
	u_int16_t icon;
	char buf[16];

	icon = GPOINTER_TO_INT(gtk_hlist_get_row_data(GTK_HLIST(widget), row));
	sprintf(buf, "%u", icon);
	gtk_entry_set_text(GTK_ENTRY(oc->icon_entry), buf);
}

static void
fontsel_ok (GtkWidget *widget, gpointer data)
{
	struct options_context *oc = (struct options_context *)data;
	char *fontn = gtk_object_get_data(GTK_OBJECT(widget), "fontn");
	GtkWidget *fontseldlg = gtk_object_get_data(GTK_OBJECT(widget), "fontseldlg");
	char *fontstr;
	unsigned int i;

	i = atoi(fontn);
	if (i < oc->nfonts) {
		fontstr = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontseldlg));
		if (fontstr) {
			if (fontn[2] == 'c')
				gtk_entry_set_text(GTK_ENTRY(oc->font_entries[i]), fontstr);
			else if (fontn[2] == 'u')
				gtk_entry_set_text(GTK_ENTRY(oc->userfont_entries[i]), fontstr);
		}
	}
	gtk_widget_destroy(fontseldlg);
}

static void
fontbtn_destroy (GtkWidget *widget, gpointer data)
{
	char *fontn = (char *)data;

	xfree(fontn);
}

static void
open_fontsel (GtkWidget *widget, gpointer data)
{
	struct options_context *oc = (struct options_context *)data;
	GtkWidget *fontseldlg;
	char *fontn = gtk_object_get_data(GTK_OBJECT(widget), "fontn");

	fontseldlg = gtk_font_selection_dialog_new(fontn+4);
	gtk_object_set_data(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseldlg)->ok_button), "fontn", fontn);
	gtk_object_set_data(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseldlg)->ok_button), "fontseldlg", fontseldlg);
	gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseldlg)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(fontsel_ok), oc);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseldlg)->cancel_button),
				  "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fontseldlg));
	gtk_widget_show_all(fontseldlg);
}

#if defined(GTK_USES_X11)
char **
x_get_charsets (unsigned int *ncsp)
{
	XFontSet fontset;
	int missing_charset_count;
	char **missing_charset_list;
	char *def_string;
	char *fontset_name = "missing_charsets";

	fontset = XCreateFontSet(GDK_DISPLAY(), fontset_name,
			    &missing_charset_list, &missing_charset_count,
			    &def_string);
	*ncsp = missing_charset_count;

	return missing_charset_list;
}

void
x_free_charsets (char **charsets)
{
      XFreeStringList(charsets);
}
#endif

static void
create_options_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	GtkWidget *window;
	GtkWidget *mainbox;
	GtkWidget *maintab;
	GtkWidget *generalbox;
	GtkWidget *optiontable1;
	GtkWidget *fontoptiontable;
	GtkWidget *fontbox;
	GtkWidget *fontlabel;
	GtkWidget *tracker_entry;
	GtkWidget *name;
	GtkWidget *tracker;
	GtkWidget *optiontable2;
	GtkWidget *showjoin;
	GtkWidget *showpart;
	GtkWidget *showchange;
	GtkWidget *optiontable3;
	GtkWidget *chatcolorframe;
	GtkWidget *chatcolorpreview;
	GtkWidget *chatbgcolorframe;
	GtkWidget *chatbgcolorpreview;
	GtkWidget *chatcolorlabel;
	GtkWidget *chatbgcolorlabel;
	GtkWidget *general;
	GtkWidget *table4;
	GtkWidget *iconlabel;
	GtkWidget *icon;
	GtkWidget *empty_notebook_page;
	GtkWidget *sound;
	GtkWidget *advanced;
	GtkWidget *savebutton;
	GtkWidget *cancelbutton;
	GtkWidget *hbuttonbox1;
	GtkWidget *nick_entry;
	GtkWidget *icon_entry;
	GtkWidget *icon_list;
	GtkWidget *scroll;
	GtkWidget *vadj;
	GtkWidget **fontbtns;
	GtkWidget **font_entries;
	GtkWidget **userfont_entries;
	GtkWidget **userfontbtns;
	unsigned int nfonts, i;
	char **charsets;
	struct options_context *oc;
	char iconstr[16];

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_OPTIONS))) {
		ghx_window_present(gwin);
		return;
	}

#if defined(GTK_USES_X11)
	charsets = x_get_charsets(&nfonts);
	if (!charsets || !nfonts) {
#endif
		nfonts = 3;
		charsets = xmalloc(sizeof(char *)*nfonts);
		charsets[0] = "Charset 0";
		charsets[1] = "Charset 1";
		charsets[2] = "Charset 2";
#if defined(GTK_USES_X11)
	}
#endif

	gwin = window_create(ghtlc, WG_OPTIONS);
	window = gwin->widget;

	changetitle(ghtlc, window, "Options");
	gtk_window_set_policy(GTK_WINDOW(window), 1, 1, 0);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	mainbox = gtk_vbox_new(0, 10);
	gtk_container_add(GTK_CONTAINER(window), mainbox);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 10);

	maintab = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(mainbox), maintab, 1, 1, 0);

	generalbox = gtk_vbox_new(0, 10);
	gtk_container_add(GTK_CONTAINER(maintab), generalbox);
	gtk_container_set_border_width(GTK_CONTAINER(generalbox), 10);

	optiontable1 = gtk_table_new(2, 2, 0);
	gtk_box_pack_start(GTK_BOX(generalbox), optiontable1, 0, 1, 0);
	gtk_table_set_row_spacings(GTK_TABLE(optiontable1), 10);
	gtk_table_set_col_spacings(GTK_TABLE(optiontable1), 5);

	nick_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(optiontable1), nick_entry, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			 (GtkAttachOptions)0, 0, 0); 
	gtk_entry_set_text(GTK_ENTRY(nick_entry), ghtlc->htlc->name);

	tracker_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(optiontable1), tracker_entry, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			 (GtkAttachOptions)0, 0, 0); 

	name = gtk_label_new("Your Name:");
	gtk_table_attach(GTK_TABLE(optiontable1), name, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),  
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	gtk_label_set_justify(GTK_LABEL(name), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(name), 0, 0.5);

	tracker = gtk_label_new("Tracker:");
	gtk_table_attach(GTK_TABLE(optiontable1), tracker, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL), 
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tracker), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(tracker), 0, 0.5);

	optiontable2 = gtk_table_new(3, 1, 0);
	gtk_box_pack_start(GTK_BOX(generalbox), optiontable2, 0, 0, 0);

	showjoin = gtk_check_button_new_with_label("Show Joins in Chat");
	gtk_table_attach(GTK_TABLE(optiontable2), showjoin, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	showpart = gtk_check_button_new_with_label("Show Parts in Chat");
	gtk_table_attach(GTK_TABLE(optiontable2), showpart, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	showchange = gtk_check_button_new_with_label("Show Changes in Chat");
	gtk_table_attach(GTK_TABLE(optiontable2), showchange, 0, 1, 2, 3,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showjoin), tty_show_user_joins);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showpart), tty_show_user_parts);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showchange), tty_show_user_changes);

	optiontable3 = gtk_table_new(2, 2, 0);
	gtk_box_pack_start(GTK_BOX(generalbox), optiontable3, 1, 1, 0);
	gtk_table_set_row_spacings(GTK_TABLE(optiontable3), 10);
	gtk_table_set_col_spacings(GTK_TABLE(optiontable3), 5);

	chatcolorframe = gtk_frame_new(0);
	gtk_table_attach(GTK_TABLE(optiontable3), chatcolorframe, 1, 2, 0, 1,
			 (GtkAttachOptions)0,
			 (GtkAttachOptions)0, 0, 0);
	gtk_widget_set_usize(chatcolorframe, 50, 20);
	gtk_frame_set_shadow_type(GTK_FRAME(chatcolorframe), GTK_SHADOW_IN);

	chatcolorpreview = gtk_preview_new(GTK_PREVIEW_COLOR);
	gtk_container_add(GTK_CONTAINER(chatcolorframe), chatcolorpreview);

	chatbgcolorframe = gtk_frame_new(0);
	gtk_table_attach(GTK_TABLE(optiontable3), chatbgcolorframe, 1, 2, 1, 2,
			 (GtkAttachOptions)0,
			 (GtkAttachOptions)0, 0, 0);
	gtk_widget_set_usize(chatbgcolorframe, 50, 20);
	gtk_frame_set_shadow_type(GTK_FRAME(chatbgcolorframe), GTK_SHADOW_IN);

	chatbgcolorpreview = gtk_preview_new(GTK_PREVIEW_COLOR);
	gtk_container_add(GTK_CONTAINER(chatbgcolorframe), chatbgcolorpreview);

	fontoptiontable = gtk_table_new(2, nfonts*2+2, 0);
	gtk_table_set_row_spacings(GTK_TABLE(fontoptiontable), 10);
	gtk_table_set_col_spacings(GTK_TABLE(fontoptiontable), 5);
	
	font_entries = xmalloc(sizeof(GtkWidget *)*nfonts);
	fontbtns = xmalloc(sizeof(GtkWidget *)*nfonts);
	userfont_entries = xmalloc(sizeof(GtkWidget *)*nfonts);
	userfontbtns = xmalloc(sizeof(GtkWidget *)*nfonts);
	fontlabel = gtk_label_new("Chat font");
	gtk_table_attach(GTK_TABLE(fontoptiontable), fontlabel, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	fontlabel = gtk_label_new("User list font");
	gtk_table_attach(GTK_TABLE(fontoptiontable), fontlabel, 0, 1, 4, 5,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	for (i = 0; i < nfonts; i++) {
		char *fontn;

		font_entries[i] = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(fontoptiontable), font_entries[i], 1, 2, 1+i, 2+i,
				 (GtkAttachOptions)(GTK_FILL|GTK_EXPAND),
				 (GtkAttachOptions)0, 0, 0);
		userfont_entries[i] = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(fontoptiontable), userfont_entries[i], 1, 2, 5+i, 6+i,
				 (GtkAttachOptions)(GTK_FILL|GTK_EXPAND),
				 (GtkAttachOptions)0, 0, 0);
		fontn = xmalloc(32);
		snprintf(fontn, 32, "%u c %s font", i, charsets[i]);
		fontbtns[i] = gtk_button_new_with_label(fontn+4);
		gtk_object_set_data(GTK_OBJECT(fontbtns[i]), "fontn", fontn);
		gtk_signal_connect(GTK_OBJECT(fontbtns[i]), "destroy",
				   GTK_SIGNAL_FUNC(fontbtn_destroy), fontn);
		gtk_table_attach(GTK_TABLE(fontoptiontable), fontbtns[i], 0, 1, 1+i, 2+i,
				 (GtkAttachOptions)(GTK_FILL),
				 (GtkAttachOptions)(GTK_FILL), 0, 0);
		fontn = xmalloc(32);
		snprintf(fontn, 32, "%u u %s font", i, charsets[i]);
		userfontbtns[i] = gtk_button_new_with_label(fontn+4);
		gtk_object_set_data(GTK_OBJECT(userfontbtns[i]), "fontn", fontn);
		gtk_signal_connect(GTK_OBJECT(userfontbtns[i]), "destroy",
				   GTK_SIGNAL_FUNC(fontbtn_destroy), fontn);
		gtk_table_attach(GTK_TABLE(fontoptiontable), userfontbtns[i], 0, 1, 5+i, 6+i,
				 (GtkAttachOptions)(GTK_FILL),
				 (GtkAttachOptions)(GTK_FILL), 0, 0);
	}
#if defined(GTK_USES_X11)
	x_free_charsets(charsets);
#else
	xfree(charsets);
#endif
	if (def_font) {
		char *p, *bp;
		int p_was_end;

		i = 0;
		for (p = bp = def_font; ; p++) {
			if (*p == ',' || (*p == 0 && *(p-1) != ',')) {
				p_was_end = *p == 0 ? 1 : 0;
				*p = 0;
				if (i >= nfonts)
					break;
				gtk_entry_set_text(GTK_ENTRY(font_entries[i]), bp);
				if (!p_was_end)
					*p = ',';
				i++;
				bp = p+1;
				if (*p == 0)
					break;
			}
		}
	}
	if (def_users_font) {
		char *p, *bp;
		int p_was_end;

		i = 0;
		for (p = bp = def_users_font; ; p++) {
			if (*p == ',' || (*p == 0 && *(p-1) != ',')) {
				p_was_end = *p == 0 ? 1 : 0;
				*p = 0;
				if (i >= nfonts)
					break;
				gtk_entry_set_text(GTK_ENTRY(userfont_entries[i]), bp);
				if (!p_was_end)
					*p = ',';
				i++;
				bp = p+1;
				if (*p == 0)
					break;
			}
		}
	}

	chatcolorlabel = gtk_label_new("Chat Text Color:");
	gtk_table_attach(GTK_TABLE(optiontable3), chatcolorlabel, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	gtk_label_set_justify(GTK_LABEL(chatcolorlabel), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(chatcolorlabel), 0, 0.5);

	chatbgcolorlabel = gtk_label_new("Chat Background Color:");
	gtk_table_attach(GTK_TABLE(optiontable3), chatbgcolorlabel, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_FILL), 0, 0);
	gtk_label_set_justify(GTK_LABEL(chatbgcolorlabel), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(chatbgcolorlabel), 0, 0.5);

	general = gtk_label_new("General");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(maintab), gtk_notebook_get_nth_page(GTK_NOTEBOOK(maintab), 0), general);
 
	table4 = gtk_table_new(2, 2, 0);
	gtk_container_add(GTK_CONTAINER(maintab), table4);
	gtk_container_set_border_width(GTK_CONTAINER(table4), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table4), 4);

	fontbox = gtk_vbox_new(0, 10);
	gtk_container_add(GTK_CONTAINER(maintab), fontbox);
	gtk_container_set_border_width(GTK_CONTAINER(fontbox), 10);
	gtk_box_pack_start(GTK_BOX(fontbox), fontoptiontable, 1, 1, 0);

	iconlabel = gtk_label_new("Icon:");
	gtk_table_attach(GTK_TABLE(table4), iconlabel, 0, 1, 0, 1,
			 (GtkAttachOptions)(0),
			 (GtkAttachOptions)0, 0, 0);

	icon_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table4), icon_entry, 1, 2, 0, 1,
			 (GtkAttachOptions)0,
			 (GtkAttachOptions)0, 0, 0); 
	sprintf(iconstr, "%u", ghtlc->htlc->icon);
	gtk_entry_set_text(GTK_ENTRY(icon_entry), iconstr);

	icon_list = gtk_hlist_new(2);
	GTK_HLIST(icon_list)->want_stipple = 1;
	gtk_hlist_set_selection_mode(GTK_HLIST(icon_list), GTK_SELECTION_EXTENDED);
	gtk_hlist_set_column_width(GTK_HLIST(icon_list), 0, 240);
	gtk_hlist_set_column_width(GTK_HLIST(icon_list), 1, 32);
	gtk_hlist_set_row_height(GTK_HLIST(icon_list), 18);
	scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(scroll) = 0;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(scroll, 232, 256);
	gtk_container_add(GTK_CONTAINER(scroll), icon_list);
	gtk_table_attach(GTK_TABLE(table4), scroll, 0, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK),
			 0, 0); 

	oc = xmalloc(sizeof(struct options_context));
	oc->ghtlc = ghtlc;
	oc->gwin = gwin;
	oc->showjoin_btn = showjoin;
	oc->showpart_btn = showpart;
	oc->showchange_btn = showchange;
	oc->nick_entry = nick_entry;
	oc->icon_entry = icon_entry;
	oc->icon_list = icon_list;
	oc->nfound = 0;
	oc->icon_high = 0;
	oc->font_entries = font_entries;
	oc->userfont_entries = userfont_entries;
	oc->nfonts = nfonts;

	for (i = 0; i < nfonts; i++) {
		gtk_signal_connect(GTK_OBJECT(fontbtns[i]), "clicked",
				   GTK_SIGNAL_FUNC(open_fontsel), (gpointer)oc);
		gtk_signal_connect(GTK_OBJECT(userfontbtns[i]), "clicked",
				   GTK_SIGNAL_FUNC(open_fontsel), (gpointer)oc);
	}
	xfree(fontbtns);
	xfree(userfontbtns);

	vadj = (GtkWidget *)gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
	gtk_signal_connect(GTK_OBJECT(vadj), "value_changed",
			   GTK_SIGNAL_FUNC(icon_list_scrolled), oc);
	gtk_signal_connect(GTK_OBJECT(icon_list), "select_row",
			   GTK_SIGNAL_FUNC(icon_row_selected), oc);

	icon = gtk_label_new("Icon");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(maintab), gtk_notebook_get_nth_page(GTK_NOTEBOOK(maintab), 1), icon);

	empty_notebook_page = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(maintab), empty_notebook_page);

	fontlabel = gtk_label_new("Fonts");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(maintab), gtk_notebook_get_nth_page(GTK_NOTEBOOK(maintab), 2), fontlabel);

	sound = gtk_label_new("Sound");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(maintab), gtk_notebook_get_nth_page(GTK_NOTEBOOK(maintab), 3), sound);

	empty_notebook_page = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(maintab), empty_notebook_page);

	advanced = gtk_label_new("Advanced");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(maintab), gtk_notebook_get_nth_page(GTK_NOTEBOOK(maintab), 4), advanced);

	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(mainbox), hbuttonbox1, 0, 0, 0);
	gtk_button_box_set_child_size(GTK_BUTTON_BOX(hbuttonbox1), 0, 22);

	savebutton = gtk_button_new_with_label("Save");
	cancelbutton = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(savebutton), "clicked",
				  GTK_SIGNAL_FUNC(options_change), (gpointer)oc);
	gtk_signal_connect_object(GTK_OBJECT(cancelbutton), "clicked", 
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)window); 

	gtk_container_add(GTK_CONTAINER(hbuttonbox1), cancelbutton);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), savebutton);

	gtk_widget_show_all(window);
}

void
open_options (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_options_window(ghtlc);
}
