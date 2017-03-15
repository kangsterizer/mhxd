#include "hx.h"
#include "hx_gtk.h"
#include "icons.h"
#include "xmalloc.h"
#include <string.h>

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

static void
post_news (GtkWidget *widget, gpointer data)
{
	struct ghx_window *gwin = (struct ghx_window *)data;
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)gtk_object_get_data(GTK_OBJECT(widget), "ghtlc");
	GtkWidget *posttext = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "posttext");
	char *postchars;

	postchars = gtk_editable_get_chars(GTK_EDITABLE(posttext), 0, -1);

	hx_post_news(ghtlc->htlc, postchars, strlen(postchars));

	g_free(postchars);

	gtk_widget_destroy(gwin->widget);
}

static void
create_post_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *okbut;
	GtkWidget *cancbut;
	GtkWidget *vbox, *hbox;
	GtkWidget *posttext;
	GtkWidget *window;

	gwin = window_create(ghtlc, WG_POST);
	wg = gwin->wg;
	window = gwin->widget;

	changetitle(ghtlc, window, "Post News");

	posttext = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(posttext), 1);
	gtk_widget_set_usize(posttext, 0, wg->height - 40);

	vbox = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), posttext, 1, 1, 0);

	hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 0);

	okbut = gtk_button_new_with_label("OK");
	gtk_object_set_data(GTK_OBJECT(okbut), "ghtlc", ghtlc);
	gtk_object_set_data(GTK_OBJECT(okbut), "posttext", posttext);
	gtk_signal_connect(GTK_OBJECT(okbut), "clicked",
			   GTK_SIGNAL_FUNC(post_news), gwin);
	cancbut = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cancbut), "clicked", 
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)window); 

	gtk_box_pack_start(GTK_BOX(hbox), okbut, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cancbut, 0, 0, 0);

	gtk_widget_show_all(window);
}

static void
open_post (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_post_window(ghtlc);
}

static void
reload_news (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	hx_get_news(ghtlc->htlc);
}

static void
news_destroy (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	ghtlc->news_postbtn = 0;
	ghtlc->news_reloadbtn = 0;
}

static void
create_news_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *window;
	GtkWidget *vscrollbar;
	GtkWidget *hbox, *vbox;
	GtkStyle *style;
	GtkWidget *posthbox;
	GtkWidget *text;
	GtkWidget *postbtn, *reloadbtn;
	GtkTooltips *tooltips;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_NEWS))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_NEWS);
	wg = gwin->wg;
	window = gwin->widget;

	gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				  GTK_SIGNAL_FUNC(news_destroy), (gpointer)ghtlc);

	changetitle(ghtlc, window, "News");

	tooltips = gtk_tooltips_new();
	postbtn = icon_button_new(ICON_NEWS, "Post News", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(postbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_post), (gpointer)ghtlc);
	reloadbtn = icon_button_new(ICON_RELOAD, "Reload News", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(reloadbtn), "clicked",
				  GTK_SIGNAL_FUNC(reload_news), (gpointer)ghtlc);

	ghtlc->news_postbtn = postbtn;
	ghtlc->news_reloadbtn = reloadbtn;

	text = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(text), 0);
	gtk_text_set_word_wrap(GTK_TEXT(text), 1);
	if (ghtlc->gchat_list) {
		style = gtk_widget_get_style(ghtlc->gchat_list->chat_output_text);
		gtk_widget_set_style(text, style);
	}
	ghtlc->news_text = text;

	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);

	vbox = gtk_vbox_new(0, 0);
	hbox = gtk_hbox_new(0, 0);
	posthbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(posthbox), reloadbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(posthbox), postbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), posthbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(text), 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_widget_show_all(window);

	gtk_widget_set_sensitive(postbtn, ghtlc->connected);
	gtk_widget_set_sensitive(reloadbtn, ghtlc->connected);
}

void
open_news (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct ghx_window *gwin;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_NEWS))) {
		ghx_window_present(gwin);
		return;
	}

	create_news_window(ghtlc);
	if (!ghtlc->htlc->news_len)
		hx_get_news(ghtlc->htlc);
	else
		hx_output.news_file(ghtlc->htlc, ghtlc->htlc->news_buf, ghtlc->htlc->news_len);
}

void
output_agreement (struct htlc_conn *htlc, const char *agreement, u_int16_t len)
{
	hx_printf_prefix(htlc, 0, INFOPREFIX, "Agreement:\n%.*s\n", len, agreement);
}

void
output_news_file (struct htlc_conn *htlc, const char *news, u_int16_t len)
{
	struct ghtlc_conn *ghtlc;
	GdkColor *fgcolor, *bgcolor;
	GtkWidget *text;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	if (!ghx_window_with_wgi(ghtlc, WG_NEWS))
		return;
	open_news(ghtlc);
	text = ghtlc->news_text;
	fgcolor = &text->style->fg[GTK_STATE_NORMAL];
	bgcolor = &text->style->bg[GTK_STATE_NORMAL];
	gtk_text_freeze(GTK_TEXT(text));
	gtk_text_set_point(GTK_TEXT(text), 0);
	gtk_text_forward_delete(GTK_TEXT(text), gtk_text_get_length(GTK_TEXT(text)));
#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, news, len, &out_p);
	if (out_len) {
		gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, out_p, out_len);
		gtk_text_thaw(GTK_TEXT(text));
		xfree(out_p);
		return;
	}
}
#endif
	gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, news, len);
	gtk_text_thaw(GTK_TEXT(text));
}

void
output_news_post (struct htlc_conn *htlc, const char *news, u_int16_t len)
{
	struct ghtlc_conn *ghtlc;
	GdkColor *fgcolor, *bgcolor;
	GtkWidget *text;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	if (!ghx_window_with_wgi(ghtlc, WG_NEWS))
		return;
	open_news(ghtlc);
	text = ghtlc->news_text;
	fgcolor = &text->style->fg[GTK_STATE_NORMAL];
	bgcolor = &text->style->bg[GTK_STATE_NORMAL];
	gtk_text_set_point(GTK_TEXT(text), 0);
	gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, news, len);
	hx_printf_prefix(htlc, 0, INFOPREFIX, "news posted\n");
}
