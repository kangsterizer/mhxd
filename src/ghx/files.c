#include "hx.h"
#include "hx_gtk.h"
#include <gdk/gdkkeysyms.h>
#include "gtk_hlist.h"
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "xmalloc.h"
#include "icons.h"
#include "cmenu.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

struct gfile_list {
	struct gfile_list *next, *prev;
	struct gfile_list *from_gfl;
	struct ghtlc_conn *ghtlc;
	struct cached_filelist *cfl;
	GtkWidget *window, *hlist;
	char path[4]; /* really longer */
};

static struct gfile_list *
gfl_new (struct ghtlc_conn *ghtlc, GtkWidget *window, GtkWidget *hlist, char *path)
{
	struct gfile_list *gfl;

	gfl = xmalloc(sizeof(struct gfile_list) + strlen(path));
	gfl->next = 0;
	gfl->prev = ghtlc->gfile_list;
	if (ghtlc->gfile_list)
		ghtlc->gfile_list->next = gfl;
	ghtlc->gfile_list = gfl;
	gfl->ghtlc = ghtlc;
	gfl->cfl = 0;
	gfl->window = window;
	gfl->hlist = hlist;
	strcpy(gfl->path, path);

	return gfl;
}

static void
gfl_delete (struct ghtlc_conn *ghtlc, struct gfile_list *gfl)
{
	if (gfl->next)
		gfl->next->prev = gfl->prev;
	if (gfl->prev)
		gfl->prev->next = gfl->next;
	if (gfl == ghtlc->gfile_list)
		ghtlc->gfile_list = gfl->prev;
	xfree(gfl);
}

void
gfl_delete_all (struct ghtlc_conn *ghtlc)
{
	struct gfile_list *gfl, *prev;

	for (gfl = ghtlc->gfile_list; gfl; gfl = prev) {
		prev = gfl->prev;
		gtk_widget_destroy(gfl->window);
	}
}

static struct gfile_list *
gfl_with_hlist (struct ghtlc_conn *ghtlc, GtkWidget *hlist)
{
	struct gfile_list *gfl;

	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (gfl->hlist == hlist)
			return gfl;
	}

	return 0;
}

static struct gfile_list *
gfl_with_path (struct ghtlc_conn *ghtlc, const char *path)
{
	struct gfile_list *gfl;

	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (!strcmp(gfl->path, path))
			return gfl;
	}

	return 0;
}

static struct gfile_list *
gfl_with_cfl (struct ghtlc_conn *ghtlc, struct cached_filelist *cfl)
{
	struct gfile_list *gfl;

	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (gfl->cfl == cfl)
			return gfl;
	}
	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (!strcmp(gfl->path, cfl->path)) {
			gfl->cfl = cfl;
			return gfl;
		}
	}

	return 0;
}

static void create_files_window (struct ghtlc_conn *ghtlc, char *path);

static void
open_folder (struct ghtlc_conn *ghtlc, struct cached_filelist *cfl, struct hl_filelist_hdr *fh)
{
	char path[4096];
	struct gfile_list *gfl;

	if (cfl->path[0] == '/' && cfl->path[1] == 0)
		snprintf(path, sizeof(path), "%c%.*s", dir_char,
			 (int)ntohs(fh->fnlen), fh->fname);
	else
		snprintf(path, sizeof(path), "%s%c%.*s",
			 cfl->path, dir_char, (int)ntohs(fh->fnlen), fh->fname);
	gfl = gfl_with_path(ghtlc, path);
	if (gfl) {
		window_present(gfl->window);
		return;
	}
	create_files_window(ghtlc, path);
	hx_list_dir(ghtlc->htlc, path, 1, 0, 0);
}

static void
open_file (struct ghtlc_conn *ghtlc, struct cached_filelist *cfl, struct hl_filelist_hdr *fh)
{
	struct htxf_conn *htxf;
	char rpath[4096], lpath[4096];

	snprintf(rpath, sizeof(rpath), "%s%c%.*s", cfl->path, dir_char, (int)ntohs(fh->fnlen), fh->fname);
	snprintf(lpath, sizeof(lpath), "%.*s", (int)ntohs(fh->fnlen), fh->fname);
	htxf = xfer_new(ghtlc->htlc, lpath, rpath, XFER_GET);
	htxf->opt.retry = 1;
}

static void
file_reload_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;

	if (!gfl->cfl)
		return;

	gtk_hlist_clear(GTK_HLIST(files_list));
	hx_list_dir(ghtlc->htlc, gfl->cfl->path, 1, 0, 0);
}

static void
file_download_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;

	if (!gfl->cfl)
		return;

	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			if (memcmp(&fh->ftype, "fldr", 4)) {
				open_file(ghtlc, gfl->cfl, fh);
			}
		}
	}
}

static void
filsel_ok (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *files_list = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "fileslist");
	GtkWidget *filsel = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "filsel");
	struct gfile_list *gfl;
	char *lpath;
	char rpath[4096];
	struct htxf_conn *htxf;

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	lpath = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filsel));
	if (!lpath)
		return;
	snprintf(rpath, sizeof(rpath), "%s%c%s", gfl->cfl->path, dir_char, basename(lpath));
	htxf = xfer_new(ghtlc->htlc, lpath, rpath, XFER_PUT);
	htxf->opt.retry = 1;

	gtk_widget_destroy(filsel);
}

static void
file_upload_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GtkWidget *filsel;
	char *title;

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	title = g_strdup_printf("Upload to %s", gfl->cfl->path);
	filsel = gtk_file_selection_new(title);
	g_free(title);
	gtk_object_set_data(GTK_OBJECT(GTK_FILE_SELECTION(filsel)->ok_button), "fileslist", files_list);
	gtk_object_set_data(GTK_OBJECT(GTK_FILE_SELECTION(filsel)->ok_button), "filsel", filsel);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filsel)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(filsel_ok), ghtlc);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(filsel)->cancel_button),
				  "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(filsel));

	gtk_widget_show_all(filsel);
}

static void
file_preview_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;
	char rpath[4096], lpath[4096];
	struct htxf_conn *htxf;

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			snprintf(rpath, sizeof(rpath), "%s%c%.*s",
				 gfl->cfl->path, dir_char, (int)ntohs(fh->fnlen), fh->fname);
			snprintf(lpath, sizeof(lpath), "preview_%.*s",
				 (int)ntohs(fh->fnlen), fh->fname);
			htxf = xfer_new(ghtlc->htlc, lpath, rpath, XFER_GET|XFER_PREVIEW);
			htxf->opt.retry = 1;
		}
	}
}

static void
file_info_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;
	char rpath[4096];

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			snprintf(rpath, sizeof(rpath), "%s%c%.*s",
				 gfl->cfl->path, dir_char, (int)ntohs(fh->fnlen), fh->fname);
			hx_get_file_info(ghtlc->htlc, rpath, 0);
		}
	}
}

static void
file_delete_btn (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *files_list = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "fileslist");
	GtkWidget *dialog = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "dialog");
	struct gfile_list *gfl;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;
	char rpath[4096];

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			snprintf(rpath, sizeof(rpath), "%s%c%.*s",
				 gfl->cfl->path, dir_char, (int)ntohs(fh->fnlen), fh->fname);
			hx_file_delete(ghtlc->htlc, rpath, 0);
		}
	}
	gtk_widget_destroy(dialog);
}

static void
file_delete_btn_ask (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GtkWidget *dialog;
	GtkWidget *btnhbox;
	GtkWidget *okbtn;
	GtkWidget *cancelbtn;

	if (!GTK_HLIST(files_list)->selection)
		return;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Delete");
	okbtn = gtk_button_new_with_label("OK");
	gtk_object_set_data(GTK_OBJECT(okbtn), "dialog", dialog);
	gtk_object_set_data(GTK_OBJECT(okbtn), "fileslist", files_list);
	gtk_signal_connect(GTK_OBJECT(okbtn), "clicked",
			   GTK_SIGNAL_FUNC(file_delete_btn), ghtlc);
	cancelbtn = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cancelbtn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(dialog));

	GTK_WIDGET_SET_FLAGS(okbtn, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(cancelbtn, GTK_CAN_DEFAULT);
	btnhbox = gtk_hbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), btnhbox);
	gtk_box_pack_start(GTK_BOX(btnhbox), okbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(btnhbox), cancelbtn, 0, 0, 0);
	gtk_widget_grab_default(okbtn);

	gtk_widget_show_all(dialog);
}

static void
files_destroy (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gfile_list *gfl = (struct gfile_list *)gtk_object_get_data(GTK_OBJECT(widget), "gfl");

	gfl_delete(ghtlc, gfl);
}

static void
file_mkdir (GtkWidget *widget, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	GtkWidget *dialog = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "dialog");
	GtkWidget *entry = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "entry");
	char *path;

	path = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	hx_mkdir(ghtlc->htlc, path);
	g_free(path);

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
file_folder_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = gfl->hlist;
	GtkWidget *dialog;
	GtkWidget *nameentry;
	GtkWidget *okbtn;
	GtkWidget *cancelbtn;
	GtkWidget *namelabel;
	GtkWidget *entryhbox;
	GtkWidget *btnhbox;
	char *path;

	gfl = gfl_with_hlist(ghtlc, files_list);
	if (!gfl || !gfl->cfl)
		return;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), "New Folder");
	entryhbox = gtk_hbox_new(0, 0);
	gtk_container_border_width(GTK_CONTAINER(dialog), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entryhbox, 0, 0, 0);
	namelabel = gtk_label_new("Name: ");
	nameentry = gtk_entry_new();
	if (gfl->cfl->path[0] == dir_char && (gfl->cfl->path[1] == dir_char || !gfl->cfl->path[1]))
		path = g_strdup_printf("%s%c", gfl->cfl->path+1, dir_char);
	else
		path = g_strdup_printf("%s%c", gfl->cfl->path, dir_char);
	gtk_entry_set_text(GTK_ENTRY(nameentry), path);
	g_free(path);
	gtk_box_pack_start(GTK_BOX(entryhbox), namelabel, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(entryhbox), nameentry, 0, 0, 0);

	okbtn = gtk_button_new_with_label("OK");
	gtk_object_set_data(GTK_OBJECT(okbtn), "entry", nameentry);
	gtk_object_set_data(GTK_OBJECT(okbtn), "dialog", dialog);
	gtk_signal_connect(GTK_OBJECT(okbtn), "clicked",
			   GTK_SIGNAL_FUNC(file_mkdir), ghtlc);
	cancelbtn = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cancelbtn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(dialog));

	GTK_WIDGET_SET_FLAGS(nameentry, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(okbtn, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(cancelbtn, GTK_CAN_DEFAULT);
	btnhbox = gtk_hbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), btnhbox);
	gtk_box_pack_start(GTK_BOX(btnhbox), okbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(btnhbox), cancelbtn, 0, 0, 0);

	gtk_widget_show_all(dialog);
}

static struct context_menu_entry file_menu_entries[] = {
	{ "get info", file_info_btn, 0, 0, 0, 0 },
	{ "download", file_download_btn, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },
	{ "delete", file_delete_btn_ask, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },
	{ "move", 0, 0, 0, 0, 0 },
	{ "link", 0, 0, 0, 0, 0 }
};

static struct context_menu *file_menu = 0;
static struct context_menu *move_menu = 0;
static struct context_menu *link_menu = 0;

static void
file_move_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct gfile_list *from_gfl = gfl->from_gfl;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = from_gfl->hlist;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;
	char topath[4096];

	snprintf(topath, sizeof(topath), "%s%c", gfl->cfl->path, dir_char);
	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			char frompath[4096];

			snprintf(frompath, sizeof(frompath), "%s%c%.*s",
				 from_gfl->cfl->path, dir_char,
				 (int)ntohs(fh->fnlen), fh->fname);
			hx_file_move(ghtlc->htlc, frompath, topath);
		}
	}
}

static void
file_link_btn (gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct gfile_list *from_gfl = gfl->from_gfl;
	struct ghtlc_conn *ghtlc = gfl->ghtlc;
	GtkWidget *files_list = from_gfl->hlist;
	GList *lp;
	gint row;
	struct hl_filelist_hdr *fh;
	char topath[4096];

	snprintf(topath, sizeof(topath), "%s%c", gfl->cfl->path, dir_char);
	for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
		row = GPOINTER_TO_INT(lp->data);
		fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
		if (fh) {
			char frompath[4096];

			snprintf(frompath, sizeof(frompath), "%s%c%.*s",
				 from_gfl->cfl->path, dir_char,
				 (int)ntohs(fh->fnlen), fh->fname);
			hx_file_link(ghtlc->htlc, frompath, topath);
		}
	}
}

static struct context_menu *
move_menu_new (struct ghtlc_conn *ghtlc, struct gfile_list *this_gfl, void (*movefn)(gpointer))
{
	struct context_menu *cmenu;
	struct context_menu_entry *cme, *cmep;
	struct gfile_list *gfl;
	unsigned int nfl = 0;

	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (gfl->cfl && gfl != this_gfl)
			nfl++;
	}
	if (!nfl)
		return 0;
	cme = xmalloc(nfl * sizeof(struct context_menu_entry));
	cmep = cme;
	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (!gfl->cfl || gfl == this_gfl)
			continue;
		gfl->from_gfl = this_gfl;
		cmep->name = xstrdup(gfl->cfl->path);
		cmep->signal_func = movefn;
		cmep->data = gfl;
		cmep->submenu = 0;
		cmep->menuitem = 0;
		cmep->hid = 0;
		cmep++;
	}
	cmenu = context_menu_new(cme, nfl);
	cmep = cme;
	for (gfl = ghtlc->gfile_list; gfl; gfl = gfl->prev) {
		if (!gfl->cfl || gfl == this_gfl)
			continue;
		xfree(cmep->name);
		cmep++;
	}
	xfree(cme);

	return cmenu;
}

static gint
file_clicked (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gfile_list *gfl;
	int row;
	int column;

	gfl = gfl_with_hlist(ghtlc, widget);
	if (!gfl)
		return 1;

	gtk_hlist_get_selection_info(GTK_HLIST(widget),
				     event->x, event->y, &row, &column);
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		struct hl_filelist_hdr *fh;

		fh = gtk_hlist_get_row_data(GTK_HLIST(widget), row);
		if (fh) {
			if (!memcmp(&fh->ftype, "fldr", 4)) {
				open_folder(ghtlc, gfl->cfl, fh);
			} else {
				open_file(ghtlc, gfl->cfl, fh);
			}
		}
		return 1;
	} else if (event->button == 3) {
		if (!file_menu)
			file_menu = context_menu_new(file_menu_entries,
						     CM_NENTRIES(file_menu_entries));
		if (move_menu)
			context_menu_delete(move_menu);
		move_menu = move_menu_new(ghtlc, gfl, file_move_btn);
		if (link_menu)
			context_menu_delete(link_menu);
		link_menu = move_menu_new(ghtlc, gfl, file_link_btn);
		context_menu_set_submenu(file_menu, 5, move_menu);
		context_menu_set_submenu(file_menu, 6, link_menu);
		if (file_menu->entries[0].data != gfl) {
			guint i;
			for (i = 0; i < file_menu->nentries; i++)
				context_menu_set_data(file_menu, i, gfl);
		}
		gtk_menu_popup(GTK_MENU(file_menu->menu), 0, 0, 0, 0,
			       event->button, event->time);
		return 1;
	}

	return 0;
}

static gint
file_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;
	GtkWidget *files_list;
	GList *lp;
	gint row;
	guint k;

	k = event->keyval;
	if (k == GDK_Return) {
		gfl = gfl_with_hlist(ghtlc, widget);
		if (!gfl)
			return 1;
		files_list = gfl->hlist;
		for (lp = GTK_HLIST(files_list)->selection; lp; lp = lp->next) {
			row = GPOINTER_TO_INT(lp->data);
			fh = gtk_hlist_get_row_data(GTK_HLIST(files_list), row);
			if (fh) {
				if (!memcmp(&fh->ftype, "fldr", 4)) {
					open_folder(ghtlc, gfl->cfl, fh);
				} else {
					open_file(ghtlc, gfl->cfl, fh);
				}
			}
		}
		return 1;
	}

	return 0;
}

static void
create_files_window (struct ghtlc_conn *ghtlc, char *path)
{
	GtkWidget *files_window;
	GtkWidget *files_list;
	GtkWidget *files_window_scroll;
	GtkWidget *reloadbtn;
	GtkWidget *downloadbtn;
	GtkWidget *uploadbtn;
	GtkWidget *folderbtn;
	GtkWidget *previewbtn;
	GtkWidget *infobtn;
	GtkWidget *deletebtn;
	GtkWidget *vbox;
	GtkWidget *hbuttonbox;
	GtkWidget *topframe;
	GtkTooltips *tooltips;
	struct gfile_list *gfl;
	static gchar *titles[] = {"Name", "Size"};

	files_list = gtk_hlist_new_with_titles(2, titles);
	gtk_hlist_set_column_auto_resize(GTK_HLIST(files_list), 0, 1);
	gtk_hlist_set_column_auto_resize(GTK_HLIST(files_list), 1, 1);
	gtk_hlist_set_column_width(GTK_HLIST(files_list), 0, 240);
	gtk_hlist_set_column_width(GTK_HLIST(files_list), 1, 40);
	gtk_hlist_set_row_height(GTK_HLIST(files_list), 18);
	gtk_hlist_set_shadow_type(GTK_HLIST(files_list), GTK_SHADOW_NONE);
	gtk_hlist_set_column_justification(GTK_HLIST(files_list), 0, GTK_JUSTIFY_LEFT);
	gtk_hlist_set_selection_mode(GTK_HLIST(files_list), GTK_SELECTION_EXTENDED);
	gtk_signal_connect(GTK_OBJECT(files_list), "button_press_event",
			   GTK_SIGNAL_FUNC(file_clicked), ghtlc);
	gtk_signal_connect(GTK_OBJECT(files_list), "key_press_event",
			   GTK_SIGNAL_FUNC(file_key_press), ghtlc);

	files_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(files_window), 1, 1, 0);
	gtk_window_set_title(GTK_WINDOW(files_window), path);
	gtk_widget_set_usize(files_window, 320, 400);

	gfl = gfl_new(ghtlc, files_window, files_list, path);
	gtk_object_set_data(GTK_OBJECT(files_window), "gfl", gfl);
	gtk_signal_connect(GTK_OBJECT(files_window), "destroy",
			   GTK_SIGNAL_FUNC(files_destroy), ghtlc);

	files_window_scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(files_window_scroll) = 0;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(files_window_scroll),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	topframe = gtk_frame_new(0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);

	tooltips = gtk_tooltips_new();
	reloadbtn = icon_button_new(ICON_RELOAD, "Reload", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(reloadbtn), "clicked",
				  GTK_SIGNAL_FUNC(file_reload_btn), (gpointer)gfl);
	downloadbtn = icon_button_new(ICON_DOWNLOAD, "Download", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(downloadbtn), "clicked",
				  GTK_SIGNAL_FUNC(file_download_btn), (gpointer)gfl);
	uploadbtn = icon_button_new(ICON_UPLOAD, "Upload", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(uploadbtn), "clicked",
				  GTK_SIGNAL_FUNC(file_upload_btn), (gpointer)gfl);
	folderbtn = icon_button_new(ICON_FOLDER, "New Folder", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(folderbtn), "clicked",
				  GTK_SIGNAL_FUNC(file_folder_btn), (gpointer)gfl);
	previewbtn = icon_button_new(ICON_PREVIEW, "Preview", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(previewbtn), "clicked",
				  GTK_SIGNAL_FUNC(file_preview_btn), (gpointer)gfl);
	infobtn = icon_button_new(ICON_INFO, "Get Info", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(infobtn), "clicked",
				  GTK_SIGNAL_FUNC(file_info_btn), (gpointer)gfl);
	deletebtn = icon_button_new(ICON_TRASH, "Delete", files_window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(deletebtn), "clicked",
				  GTK_SIGNAL_FUNC(file_delete_btn_ask), (gpointer)gfl);

	hbuttonbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), downloadbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), uploadbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), deletebtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), folderbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), previewbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), infobtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), reloadbtn, 0, 0, 2);

	vbox = gtk_vbox_new(0, 0);
	gtk_widget_set_usize(vbox, 240, 400);
	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox);
	gtk_box_pack_start(GTK_BOX(vbox), topframe, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(files_window_scroll), files_list);
	gtk_box_pack_start(GTK_BOX(vbox), files_window_scroll, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(files_window), vbox);

	gtk_widget_show_all(files_window);

	keyaccel_attach(ghtlc, files_window);
}

void
open_files (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gfile_list *gfl;
	char dir_str[2];

	dir_str[0] = dir_char;
	dir_str[1] = 0;
	if ((gfl = gfl_with_path(ghtlc, dir_str))) {
		window_present(gfl->window);
		return;
	}
	create_files_window(ghtlc, dir_str);
	hx_list_dir(ghtlc->htlc, dir_str, 1, 0, 0);
}

char *
strcasestr_len (char *haystack, char *needle, size_t len)
{
	char *p, *startn = 0, *np = 0, *end = haystack + len;

	for (p = haystack; p < end; p++) {
		if (np) {
			if (toupper(*p) == toupper(*np)) {
				if (!*++np)
					return startn;
			} else
				np = 0;
		} else if (toupper(*p) == toupper(*needle)) {
			np = needle + 1;
			startn = p;
		}
	}

	return 0;
}

static u_int16_t
icon_of_fh (struct hl_filelist_hdr *fh)
{
	u_int16_t icon;

	if (!memcmp(&fh->ftype, "fldr", 4)) {
		u_int16_t len = ntohs(fh->fnlen);
		if (strcasestr_len(fh->fname, "DROP BOX", len)
		    || strcasestr_len(fh->fname, "UPLOAD", len))
			icon = ICON_FOLDER_IN;
		else
			icon = ICON_FOLDER;
	} else if (!memcmp(&fh->ftype, "JPEG", 4)
		 || !memcmp(&fh->ftype, "PNGf", 4)
		 || !memcmp(&fh->ftype, "GIFf", 4)
		 || !memcmp(&fh->ftype, "PICT", 4))
		icon = ICON_FILE_IMAGE;
	else if (!memcmp(&fh->ftype, "MPEG", 4)
		 || !memcmp(&fh->ftype, "MPG ", 4)
		 || !memcmp(&fh->ftype, "AVI ", 4)
		 || !memcmp(&fh->ftype, "MooV", 4))
		icon = ICON_FILE_MOOV;
	else if (!memcmp(&fh->ftype, "MP3 ", 4))
		icon = ICON_FILE_NOTE;
	else if (!memcmp(&fh->ftype, "ZIP ", 4))
		icon = ICON_FILE_ZIP;
	else if (!memcmp(&fh->ftype, "SIT", 3))
		icon = ICON_FILE_SIT;
	else if (!memcmp(&fh->ftype, "APPL", 4))
		icon = ICON_FILE_APPL;
	else if (!memcmp(&fh->ftype, "rohd", 4))
		icon = ICON_FILE_DISK;
	else if (!memcmp(&fh->ftype, "HTft", 4))
		icon = ICON_FILE_HTft;
	else if (!memcmp(&fh->ftype, "alis", 4))
		icon = ICON_FILE_alis;
	else
		icon = ICON_FILE;

	return icon;
}

void
output_file_list (struct htlc_conn *htlc, struct cached_filelist *cfl)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *files_list;
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	u_int16_t icon;
	gint row;
 	gchar *nulls[2] = {0, 0};
	char humanbuf[LONGEST_HUMAN_READABLE+1], *sizstr;
	char namstr[512];
	u_int32_t len;
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	gfl = gfl_with_cfl(ghtlc, cfl);
	if (!gfl)
		return;
	files_list = gfl->hlist;

	gtk_hlist_freeze(GTK_HLIST(files_list));
	gtk_hlist_clear(GTK_HLIST(files_list));
	for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
	     (char *)fh += ntohs(fh->len) + SIZEOF_HL_DATA_HDR) {
		row = gtk_hlist_append(GTK_HLIST(files_list), nulls);
		gtk_hlist_set_row_data(GTK_HLIST(files_list), row, fh);
		icon = icon_of_fh(fh);
		pixc = load_icon(files_list, icon, &icon_files, 0, 0);
		if (pixc) {
			pixmap = pixc->pixmap;
			mask = pixc->mask;
		} else {
			pixmap = 0;
			mask = 0;
		}
		len = ntohs(fh->fnlen);
		if (len > sizeof(namstr)-1)
			len = sizeof(namstr)-1;
		memcpy(namstr, fh->fname, len);
		namstr[len] = 0;
		if (!memcmp(&fh->ftype, "fldr", 4)) {
			sizstr = humanbuf;
			sprintf(sizstr, "(%u)", ntohl(fh->fsize));
		} else {
			sizstr = human_size(ntohl(fh->fsize), humanbuf);
		}
		gtk_hlist_set_text(GTK_HLIST(files_list), row, 1, sizstr);
		if (!pixmap)
			gtk_hlist_set_text(GTK_HLIST(files_list), row, 0, namstr);
		else
			gtk_hlist_set_pixtext(GTK_HLIST(files_list), row, 0, namstr, 34, pixmap, mask);
	}
	gtk_hlist_thaw(GTK_HLIST(files_list));
}

void
file_info (struct htlc_conn *htlc, const char *icon, const char *type, const char *crea,
	   u_int32_t size, const char *name, const char *created, const char *modified,
	   const char *comment)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *info_window;
	GtkWidget *vbox;
	GtkWidget *name_hbox;
	GtkWidget *size_hbox;
	GtkWidget *type_hbox;
	GtkWidget *crea_hbox;
	GtkWidget *created_hbox;
	GtkWidget *modified_hbox;
	GtkWidget *size_label;
	GtkWidget *type_label;
	GtkWidget *crea_label;
	GtkWidget *created_label;
	GtkWidget *modified_label;
	GtkWidget *icon_pmap;
	GtkWidget *name_entry;
	GtkWidget *comment_text;
	GtkStyle *style;
	char infotitle[1024];
	char buf[1024];
	char humanbuf[LONGEST_HUMAN_READABLE+1];
	struct hl_filelist_hdr *fh;
	u_int16_t iconid;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(info_window), 1, 1, 0);
	gtk_widget_set_usize(info_window, 256, 0);

	snprintf(infotitle, sizeof(infotitle), "File Info: %s", name);
	gtk_window_set_title(GTK_WINDOW(info_window), infotitle); 

	fh = (struct hl_filelist_hdr *)buf;
	fh->ftype = *((u_int32_t *)icon);
	fh->fnlen = htons(strlen(name));
	strlcpy(fh->fname, name, 768);
	iconid = icon_of_fh(fh);
	icon_pmap = icon_pixmap(info_window, iconid);
	name_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name_entry), name);

	comment_text = gtk_text_new(0, 0);
	if (ghtlc->gchat_list) {
		style = gtk_widget_get_style(ghtlc->gchat_list->chat_output_text);
		gtk_widget_set_style(name_entry, style);
		gtk_widget_set_style(comment_text, style);
	}
	gtk_text_insert(GTK_TEXT(comment_text), 0, 0, 0, comment, -1);

	sprintf(buf, "Size: %s (%u bytes)", human_size(size, humanbuf), size);
	size_label = gtk_label_new(buf);
	sprintf(buf, "Type: %s", type);
	type_label = gtk_label_new(buf);
	sprintf(buf, "Creator: %s", crea);
	crea_label = gtk_label_new(buf);
	sprintf(buf, "Created: %s", created);
	created_label = gtk_label_new(buf);
	sprintf(buf, "Modified: %s", modified);
	modified_label = gtk_label_new(buf);

	size_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(size_hbox), size_label, 0, 0, 2);
	type_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(type_hbox), type_label, 0, 0, 2);
	crea_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(crea_hbox), crea_label, 0, 0, 2);
	created_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(created_hbox), created_label, 0, 0, 2);
	modified_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(modified_hbox), modified_label, 0, 0, 2);

	name_hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(name_hbox), icon_pmap, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(name_hbox), name_entry, 1, 1, 2);
	vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), name_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), size_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), type_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), crea_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), created_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), modified_hbox, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), comment_text, 1, 1, 2);
	gtk_container_add(GTK_CONTAINER(info_window), vbox);

	gtk_widget_show_all(info_window);

	keyaccel_attach(ghtlc, info_window);
}

static GtkWidget *
create_text_widget (struct ghtlc_conn *ghtlc)
{
	GtkWidget *preview_hbox;
	GtkWidget *vscrollbar;
	GtkStyle *style;
	GtkWidget *text;

	text = gtk_text_new(0, 0);
	gtk_text_set_editable(GTK_TEXT(text), 0);
	gtk_text_set_word_wrap(GTK_TEXT(text), 1);
	if (ghtlc->gchat_list) {
		style = gtk_widget_get_style(ghtlc->gchat_list->chat_output_text);
		gtk_widget_set_style(text, style);
	}
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
	preview_hbox = ghtlc->preview_hbox;
	gtk_box_pack_start(GTK_BOX(preview_hbox), GTK_WIDGET(text), 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(preview_hbox), vscrollbar, 0, 0, 0);
	gtk_widget_show(text);
	gtk_widget_show(vscrollbar);

	return text;
}

static void
preview_destroy (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
}

static void
create_preview_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *window;
	GtkWidget *hbox, *vbox;
	GtkWidget *posthbox;
	GtkWidget *prevbtn, *nextbtn;
	GtkTooltips *tooltips;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_PREVIEW))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_PREVIEW);
	wg = gwin->wg;
	window = gwin->widget;

	gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				  GTK_SIGNAL_FUNC(preview_destroy), (gpointer)ghtlc);

	changetitle(ghtlc, window, "Preview");

	tooltips = gtk_tooltips_new();
	prevbtn = icon_button_new(ICON_UPLOAD, "Previous", window, tooltips);
	nextbtn = icon_button_new(ICON_DOWNLOAD, "Next", window, tooltips);
	vbox = gtk_vbox_new(0, 0);
	hbox = gtk_hbox_new(0, 0);
	posthbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(posthbox), prevbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(posthbox), nextbtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), posthbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	ghtlc->preview_hbox = hbox;

	gtk_widget_show_all(window);
}

void
open_file_preview (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct ghx_window *gwin;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_PREVIEW))) {
		ghx_window_present(gwin);
		return;
	}

	create_preview_window(ghtlc);
}

void
output_file_preview (struct htlc_conn *htlc, const char *filename, const char *type)
{
	struct ghtlc_conn *ghtlc;
	GdkColor *fgcolor, *bgcolor;
	GtkWidget *text;
	char *databuf;
	size_t len;
	int fd;

	if (!strcmp(type, "TEXT")) {
		struct stat sb;

		fd = SYS_open(filename, O_RDONLY, 0);
		if (fd < 0)
			return;
		if (fstat(fd, &sb))
			return;
		len = sb.st_size;
		databuf = xmalloc(len);
		memset(databuf, 0, len);
		len = read(fd, databuf, len);
		close(fd);
	} else {
		/* Try ImageMagick */
		char cmd[4096];
		snprintf(cmd, sizeof(cmd), "display %s &", filename);
		system(cmd);
		return;
	}

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	open_file_preview(ghtlc);
	text = create_text_widget(ghtlc);
	fgcolor = &text->style->fg[GTK_STATE_NORMAL];
	bgcolor = &text->style->bg[GTK_STATE_NORMAL];
	gtk_text_freeze(GTK_TEXT(text));
	gtk_text_set_point(GTK_TEXT(text), 0);
	gtk_text_forward_delete(GTK_TEXT(text), gtk_text_get_length(GTK_TEXT(text)));
#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, databuf, len, &out_p);
	if (out_len) {
		gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, out_p, out_len);
		gtk_text_thaw(GTK_TEXT(text));
		xfree(out_p);
		return;
	}
}
#endif
	gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, databuf, len);
	gtk_text_thaw(GTK_TEXT(text));
}
