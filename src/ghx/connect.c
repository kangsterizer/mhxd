#include "hx.h"
#include "hx_gtk.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "xmalloc.h"

extern void create_chat_window (struct ghtlc_conn *ghtlc, struct hx_chat *chat);
extern void create_toolbar_window (struct ghtlc_conn *ghtlc);

struct connect_context {
	struct ghtlc_conn *ghtlc;
	GtkWidget *connect_window;
	GtkWidget *address_entry;
	GtkWidget *login_entry;
	GtkWidget *password_entry;
	GtkWidget *cipher_entry;
	GtkWidget *secure_toggle;
	GtkWidget *compress_entry;
};

void
connect_set_entries (struct connect_context *cc, const char *address, const char *login, const char *password)
{
	if (address)
		gtk_entry_set_text(GTK_ENTRY(cc->address_entry), address);
	if (login)
		gtk_entry_set_text(GTK_ENTRY(cc->login_entry), login);
	if (password)
		gtk_entry_set_text(GTK_ENTRY(cc->password_entry), password);
}

static void
server_connect (gpointer data)
{
	struct connect_context *cc = (struct connect_context *)data;
	struct ghtlc_conn *ghtlc = cc->ghtlc;
	char *server;
	char *login;
	char *pass;
	char *serverstr, *p;
	char *cipher;
	char *compress;
	int secure;
	u_int16_t clen;
	u_int16_t port = 5500;
	struct htlc_conn *htlc;

	server = gtk_entry_get_text(GTK_ENTRY(cc->address_entry));
	login = gtk_entry_get_text(GTK_ENTRY(cc->login_entry));
	pass = gtk_entry_get_text(GTK_ENTRY(cc->password_entry));
	cipher = gtk_entry_get_text(GTK_ENTRY(cc->cipher_entry));
	compress = gtk_entry_get_text(GTK_ENTRY(cc->compress_entry));
	secure = GTK_TOGGLE_BUTTON(cc->secure_toggle)->active;

	serverstr = g_strdup(server);
#ifndef CONFIG_IPV6
	p = strchr(serverstr, ':');
	if (p) {
		*p++ = 0;
		if (*p)
			port = strtoul(p, 0, 0);
	}
#endif

	htlc = xmalloc(sizeof(struct htlc_conn));
	memset(htlc, 0, sizeof(struct htlc_conn));
	strcpy(htlc->name, ghtlc->htlc->name);
	htlc->icon = ghtlc->htlc->icon;
#ifdef CONFIG_CIPHER
	clen = strlen(cipher);
	if (clen >= sizeof(htlc->cipheralg))
		clen = sizeof(htlc->cipheralg)-1;
	memcpy(htlc->cipheralg, cipher, clen);
	htlc->cipheralg[clen] = 0;
#endif
#ifdef CONFIG_COMPRESS
	clen = strlen(compress);
	if (clen >= sizeof(htlc->compressalg))
		clen = sizeof(htlc->compressalg)-1;
	memcpy(htlc->compressalg, compress, clen);
	htlc->compressalg[clen] = 0;
#endif
	ghtlc = ghtlc_conn_new(htlc);
	create_toolbar_window(ghtlc);
	create_chat_window(ghtlc, 0);
	hx_connect(htlc, serverstr, port, htlc->name, htlc->icon, login, pass, secure);
	g_free(serverstr);

	gtk_widget_destroy(cc->connect_window);
	xfree(cc);
}

static void
open_bookmark (GtkWidget *widget, gpointer data)
{
	struct connect_context *cc = (struct connect_context *)gtk_object_get_data(GTK_OBJECT(widget), "cc");
	char *file = (char *)data;
	FILE *fp;
	char line1[64];
	char line2[64];
	char line3[64];
	char path[MAXPATHLEN], buf[MAXPATHLEN];

	snprintf(buf, sizeof(buf), "~/.hx/bookmarks/%s", file);
	expand_tilde(path, buf);
	xfree(file);

	fp = fopen(path, "r");
	if (fp) {
		line1[0] = line2[0] = line3[0];
		fgets(line1, 64, fp);
		fgets(line2, 64, fp);
		fgets(line3, 64, fp);
		line1[strlen(line1)-1] = 0;
		if (strlen(line2))
			line2[strlen(line2)-1] = 0;
		if (strlen(line3))
			line3[strlen(line3)-1] = 0;
		connect_set_entries(cc, line1, line2, line3);
		fclose(fp);
	} else {
		g_message("No such file.  Bummer!");
	}
}

static void
list_bookmarks (GtkWidget *menu, struct connect_context *cc)
{
	struct dirent *de;
	char *file;
	char path[MAXPATHLEN];
	GtkWidget *item;
	DIR *dir;

	expand_tilde(path, "~/.hx/bookmarks");
	dir = opendir(path);
	if (!dir)
		return;
	while ((de = readdir(dir))) {
		if (*de->d_name != '.') {
			file = xstrdup(de->d_name);
			item = gtk_menu_item_new_with_label(file);
			gtk_menu_append(GTK_MENU(menu), item);
			gtk_object_set_data(GTK_OBJECT(item), "cc", cc);
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(open_bookmark), file);
		}
	}
	closedir(dir);
}

static void
cancel_save (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "dialog");

	gtk_widget_destroy(dialog);
}

static void
bookmark_save (GtkWidget *widget, gpointer data)
{
	struct connect_context *cc = (struct connect_context *)data;
	GtkWidget *name_entry = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "name");
	GtkWidget *dialog = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "dialog");
	char *server = gtk_entry_get_text(GTK_ENTRY(cc->address_entry));
	char *login = gtk_entry_get_text(GTK_ENTRY(cc->login_entry));
	char *pass = gtk_entry_get_text(GTK_ENTRY(cc->password_entry));
	char *home = getenv("HOME");
	char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
	char *path = g_strdup_printf("%s/.hx/bookmarks/%s", home, name);
	char *dir = g_strdup_printf("%s/.hx/bookmarks/", home);
	FILE *fp;

	fp = fopen(path, "w");
	if (!fp) {
		SYS_mkdir(dir, 0700);
		fp = fopen(path, "w");
	}
	
	if (!fp) {
		char *basedir = g_strdup_printf("%s/.hx", home);
		SYS_mkdir(basedir, 0700);
		SYS_mkdir(dir, 0700);
		fp = fopen(path, "w");
		g_free(basedir);
	}
	
	if (!fp) {
		/* Give up */
		return;
	}
	
	fprintf(fp, "%s\n", server);
	fprintf(fp, "%s\n", login);
	fprintf(fp, "%s\n", pass);
	fclose(fp);
	g_free(path);
	g_free(dir);

	gtk_widget_destroy(dialog);
}

static void
save_dialog (gpointer data)
{
	struct connect_context *cc = (struct connect_context *)data;
	GtkWidget *dialog;
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *name_entry;
	GtkWidget *label;
	GtkWidget *hbox;

	dialog = gtk_dialog_new();
	ok = gtk_button_new_with_label("OK");
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	cancel = gtk_button_new_with_label("Cancel");
	name_entry = gtk_entry_new();
	hbox = gtk_hbox_new(0,0);
	label = gtk_label_new("Name:");

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), ok, 0,0, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), cancel, 0,0, 0);
	gtk_object_set_data(GTK_OBJECT(cancel), "dialog", dialog);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(cancel_save), cc);
	gtk_object_set_data(GTK_OBJECT(ok), "name", name_entry);
	gtk_object_set_data(GTK_OBJECT(ok), "dialog", dialog);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   GTK_SIGNAL_FUNC(bookmark_save), cc);

	gtk_widget_grab_default(ok);

	gtk_widget_show_all(dialog);
}

static void
close_connect_window (gpointer data)
{
	struct connect_context *cc = (struct connect_context *)data;

	gtk_widget_destroy(cc->connect_window);
	xfree(cc);
}

struct connect_context *
create_connect_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	GtkWidget *connect_window;
	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *cipher_label;
	GtkWidget *cipher_entry;
	GtkWidget *compress_label;
	GtkWidget *compress_entry;
	GtkWidget *secure_toggle;
	GtkWidget *help_label;
	GtkWidget *frame1;
	GtkWidget *table1;
	GtkWidget *server_label;
	GtkWidget *login_label;
	GtkWidget *pass_label;
	GtkWidget *address_entry;
	GtkWidget *login_entry;
	GtkWidget *password_entry;
	GtkWidget *button_connect;
	GtkWidget *button_cancel;
	GtkWidget *bookmarkmenu;
	GtkWidget *bookmarkmenu_menu;
	GtkWidget *hbuttonbox1;
	GtkWidget *save_button;
	struct connect_context *cc;

	gwin = window_create(ghtlc, WG_CONNECT);
	connect_window = gwin->widget;

	gtk_window_set_title(GTK_WINDOW(connect_window), "Connect");
	gtk_window_set_position(GTK_WINDOW(connect_window), GTK_WIN_POS_CENTER);

	vbox1 = gtk_vbox_new(0, 10);
	gtk_container_add(GTK_CONTAINER(connect_window), vbox1);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 10);

	help_label = gtk_label_new("Enter the server address, and if you have an account, your login and password. If not, leave the login and password blank.");
	gtk_box_pack_start(GTK_BOX(vbox1), help_label, 0, 1, 0);
	gtk_label_set_justify(GTK_LABEL(help_label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap(GTK_LABEL(help_label), 1);
	gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5); 

	frame1 = gtk_frame_new(0);
	gtk_box_pack_start(GTK_BOX(vbox1), frame1, 1, 1, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_IN);

	table1 = gtk_table_new(4, 3, 0);
	gtk_container_add(GTK_CONTAINER(frame1), table1);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 5);

	server_label = gtk_label_new("Server:");
	gtk_table_attach(GTK_TABLE(table1), server_label, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(server_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(server_label), 0, 0.5);

	login_label = gtk_label_new("Login:");
	gtk_table_attach(GTK_TABLE(table1), login_label, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(login_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(login_label), 0, 0.5);

	pass_label = gtk_label_new("Password:");
	gtk_table_attach(GTK_TABLE(table1), pass_label, 0, 1, 2, 3,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(pass_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(pass_label), 0, 0.5);

	address_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table1), address_entry, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			 (GtkAttachOptions)0, 0, 0); 

	login_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table1), login_entry, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	password_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(password_entry), 0);
	gtk_table_attach(GTK_TABLE(table1), password_entry, 1, 2, 2, 3,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);

	secure_toggle = gtk_check_button_new_with_label("Secure");
#ifdef CONFIG_HOPE
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(secure_toggle), 1);
#else
	gtk_widget_set_sensitive(secure_toggle, 0);
#endif
	cipher_label = gtk_label_new("Cipher:");
	cipher_entry = gtk_entry_new();
#ifdef CONFIG_CIPHER
	gtk_entry_set_text(GTK_ENTRY(cipher_entry), ghtlc->htlc->cipheralg);
#else
	gtk_widget_set_sensitive(cipher_entry, 0);
#endif
	compress_label = gtk_label_new("Compress:");
	compress_entry = gtk_entry_new();
#ifdef CONFIG_COMPRESS
	gtk_entry_set_text(GTK_ENTRY(compress_entry), ghtlc->htlc->compressalg);
#else
	gtk_widget_set_sensitive(compress_entry, 0);
#endif
	gtk_table_attach(GTK_TABLE(table1), secure_toggle, 0, 1, 3, 4,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cipher_label, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbox), cipher_entry, 0, 0, 2);
	gtk_table_attach(GTK_TABLE(table1), hbox, 1, 2, 3, 4,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);
	hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), compress_label, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbox), compress_entry, 0, 0, 2);
	gtk_table_attach(GTK_TABLE(table1), hbox, 1, 2, 4, 5,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)0, 0, 0);

	cc = xmalloc(sizeof(struct connect_context));
	cc->ghtlc = ghtlc;
	cc->connect_window = connect_window;
	cc->address_entry = address_entry;
	cc->login_entry = login_entry;
	cc->password_entry = password_entry;
	cc->cipher_entry = cipher_entry;
	cc->compress_entry = compress_entry;
	cc->secure_toggle = secure_toggle;

	bookmarkmenu = gtk_option_menu_new();
	gtk_table_attach(GTK_TABLE(table1), bookmarkmenu, 2, 3, 0, 1,
			 (GtkAttachOptions)0,
			 (GtkAttachOptions)0, 0, 0);
	bookmarkmenu_menu = gtk_menu_new(); 
	list_bookmarks(bookmarkmenu_menu, cc);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bookmarkmenu), bookmarkmenu_menu);

	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox1), hbuttonbox1, 1, 1, 0);

	save_button = gtk_button_new_with_label("Save...");
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), save_button);
	GTK_WIDGET_SET_FLAGS(save_button, GTK_CAN_DEFAULT);
	gtk_signal_connect_object(GTK_OBJECT(save_button), "clicked",
				  GTK_SIGNAL_FUNC(save_dialog), (gpointer)cc);
  
	button_cancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(button_cancel), "clicked",
				  GTK_SIGNAL_FUNC(close_connect_window), (gpointer)cc);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), button_cancel);
	GTK_WIDGET_SET_FLAGS(button_cancel, GTK_CAN_DEFAULT);
  
	button_connect = gtk_button_new_with_label("Connect");
	gtk_signal_connect_object(GTK_OBJECT(button_connect), "clicked",
				  GTK_SIGNAL_FUNC(server_connect), (gpointer)cc);
	gtk_container_add(GTK_CONTAINER (hbuttonbox1), button_connect);
	GTK_WIDGET_SET_FLAGS(button_connect, GTK_CAN_DEFAULT);

	gtk_signal_connect_object(GTK_OBJECT(address_entry), "activate",
				  GTK_SIGNAL_FUNC(server_connect), (gpointer)cc);
	gtk_signal_connect_object(GTK_OBJECT(login_entry), "activate",
				  GTK_SIGNAL_FUNC(server_connect), (gpointer)cc);
	gtk_signal_connect_object(GTK_OBJECT(password_entry), "activate",
				  GTK_SIGNAL_FUNC(server_connect), (gpointer)cc);

	gtk_widget_show_all(connect_window);

	return cc;
}

void
open_connect (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_connect_window(ghtlc);
}

