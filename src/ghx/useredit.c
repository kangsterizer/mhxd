#include "hx.h"
#include "hx_gtk.h"
#include "xmalloc.h"
#include <string.h>

struct access_name {
	int bitno;
	char *name;
} access_names[] = {
	{ -1, "File" },
	{ 1, "upload files" },
	{ 2, "download fIles" },
	{ 38, "upload folders" },
	{ 39, "download folders" },
	{ 4, "move files" },
	{ 8, "move folders" },
	{ 5, "create folders" },
	{ 0, "delete files" },
	{ 6, "delete folders" },
	{ 3, "rename files" },
	{ 7, "rename folders" },
	{ 28, "comment files" },
	{ 29, "comment folders" },
	{ 31, "make aliases" },
	{ 25, "upload anywhere" },
	{ 30, "view drop boxes" },
	{ -1, "Chat" },
	{ 9, "read chat" },
	{ 10, "send chat" },
	{ 40, "send messages" },
	{ -1, "News" },
	{ 20, "read news" },
	{ 21, "post news" },
	{ 33, "delete articles" },
	{ 34, "create categories" },
	{ 35, "delete categories" },
	{ 36, "create news bundles" },
	{ 37, "delete news bundles" },
	{ -1, "User" },
	{ 14, "create users" },
	{ 15, "delete users" },
	{ 16, "read users" },
	{ 17, "modify users" },
	{ 22, "disconnect users" },
	{ 23, "not be disconnected" },
	{ 24, "get user info" },
	{ 26, "use any name" },
	{ 27, "not be shown agreement" },
	{ -1, "Admin" },
	{ 32, "broadcast" },
};
#define NACCESS	36

static int
test_bit (char *buf, int bitno)
{
	char c, m;
	c = buf[bitno / 8];
	bitno = bitno % 8;
	bitno = 7 - bitno;
	if (!bitno)
		m = 1;
	else {
		m = 2;
		while (--bitno)
			m *= 2;
	}

	return c & m;
}

static void
inverse_bit (char *buf, int bitno)
{
	char *p, c, m;
	p = &buf[bitno / 8];
	c = *p;
	bitno = bitno % 8;
	bitno = 7 - bitno;
	if (!bitno)
		m = 1;
	else {
		m = 2;
		while (--bitno)
			m *= 2;
	}
	if (c & m)
		*p = c & ~m;
	else
		*p = c | m;
}

struct access_widget {
	int bitno;
	GtkWidget *widget;
};

struct useredit_session {
	struct ghtlc_conn *ghtlc;
	char access_buf[8];
	char name[32];
	char login[32];
	char pass[32];
	GtkWidget *window;
	GtkWidget *name_entry;
	GtkWidget *login_entry;
	GtkWidget *pass_entry;
	struct access_widget access_widgets[NACCESS];
};

static void
user_open (void *__uesp, const char *name, const char *login, const char *pass, const struct hl_access_bits *access)
{
	struct useredit_session *ues = (struct useredit_session *)__uesp;
	unsigned int i;
	int on;

	gtk_entry_set_text(GTK_ENTRY(ues->name_entry), name);
	gtk_entry_set_text(GTK_ENTRY(ues->login_entry), login);
	gtk_entry_set_text(GTK_ENTRY(ues->pass_entry), pass);
	strcpy(ues->name, name);
	strcpy(ues->login, login);
	strcpy(ues->pass, pass);
	memcpy(ues->access_buf, access, 8);
	for (i = 0; i < NACCESS; i++) {
		on = test_bit(ues->access_buf, ues->access_widgets[i].bitno);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ues->access_widgets[i].widget), on);
	}
}

static void
useredit_login_activate (GtkWidget *widget, gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;
	char *login;
	size_t len;

	login = gtk_entry_get_text(GTK_ENTRY(widget));
	if (ues->ghtlc->htlc)
		hx_useredit_open(ues->ghtlc->htlc, login, user_open, ues);
	len = strlen(login);
	if (len > 31)
		len = 31;
	memcpy(ues->login, login, len);
	ues->login[len] = 0;
}

static void
useredit_name_activate (GtkWidget *widget, gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;
	char *name;
	size_t len;

	name = gtk_entry_get_text(GTK_ENTRY(widget));
	len = strlen(name);
	if (len > 31)
		len = 31;
	memcpy(ues->name, name, len);
	ues->name[len] = 0;
}

static void
useredit_pass_activate (GtkWidget *widget, gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;
	char *pass;
	size_t len;

	pass = gtk_entry_get_text(GTK_ENTRY(widget));
	len = strlen(pass);
	if (len > 31)
		len = 31;
	memcpy(ues->pass, pass, len);
	ues->pass[len] = 0;
}

static void
useredit_chk_activate (GtkWidget *widget, gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;
	unsigned int i;
	int bitno;

	for (i = 0; i < NACCESS; i++) {
		if (ues->access_widgets[i].widget == widget)
			break;
	}
	if (i == NACCESS)
		return;
	bitno = ues->access_widgets[i].bitno;
	if (GTK_TOGGLE_BUTTON(widget)->active) {
		if (!test_bit(ues->access_buf, bitno))
			inverse_bit(ues->access_buf, bitno);
	} else {
		if (test_bit(ues->access_buf, bitno))
			inverse_bit(ues->access_buf, bitno);
	}
}

static void
useredit_save (gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;

	useredit_login_activate(ues->login_entry, data);
	useredit_name_activate(ues->name_entry, data);
	useredit_pass_activate(ues->pass_entry, data);
	if (ues->ghtlc->htlc)
		hx_useredit_create(ues->ghtlc->htlc, ues->login, ues->pass,
				   ues->name, (struct hl_access_bits *)ues->access_buf);
}

static void
useredit_delete (gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;

	if (ues->ghtlc->htlc)
		hx_useredit_delete(ues->ghtlc->htlc, ues->login);
}

static void
useredit_close (gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;

	gtk_widget_destroy(ues->window);
}

static void
useredit_destroy (gpointer data)
{
	struct useredit_session *ues = (struct useredit_session *)data;

	xfree(ues);
}

void
create_useredit_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	GtkWidget *window;
	GtkWidget *usermod_scroll;
	GtkWidget *wvbox;
	GtkWidget *avbox;
	GtkWidget *vbox = 0;
	GtkWidget *frame;
	GtkWidget *info_frame;
	GtkWidget *chk;
	GtkWidget *name_entry;
	GtkWidget *name_label;
	GtkWidget *login_entry;
	GtkWidget *login_label;
	GtkWidget *pass_entry;
	GtkWidget *pass_label;
	GtkWidget *info_table;
	GtkWidget *btnhbox;
	GtkWidget *savebtn;
	GtkWidget *delbtn;
	GtkWidget *closebtn;
	unsigned int i, awi, nframes = 0;
	struct useredit_session *ues;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_USEREDIT))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_USEREDIT);
	window = gwin->widget;

	changetitle(ghtlc, window, "User Editor");

	ues = xmalloc(sizeof(struct useredit_session));
	memset(ues, 0, sizeof(struct useredit_session));
	ues->ghtlc = ghtlc;
	ues->window = window;
	gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				  GTK_SIGNAL_FUNC(useredit_destroy), (gpointer)ues);

	wvbox = gtk_vbox_new(0, 0);

	savebtn = gtk_button_new_with_label("Save");
	gtk_signal_connect_object(GTK_OBJECT(savebtn), "clicked",
			   GTK_SIGNAL_FUNC(useredit_save), (gpointer)ues);
	delbtn = gtk_button_new_with_label("Delete User");
	gtk_signal_connect_object(GTK_OBJECT(delbtn), "clicked",
				  GTK_SIGNAL_FUNC(useredit_delete), (gpointer)ues);
	closebtn = gtk_button_new_with_label("Close");
	gtk_signal_connect_object(GTK_OBJECT(closebtn), "clicked",
				  GTK_SIGNAL_FUNC(useredit_close), (gpointer)ues);
	btnhbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(btnhbox), savebtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(btnhbox), delbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(btnhbox), closebtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(wvbox), btnhbox, 0, 0, 2);

	info_frame = gtk_frame_new("User Info");
	info_table = gtk_table_new(3, 2, 0);
	gtk_container_add(GTK_CONTAINER(info_frame), info_table);
	login_entry = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(login_entry), "activate",
			   GTK_SIGNAL_FUNC(useredit_login_activate), ues);
	login_label = gtk_label_new("Login:");
	name_entry = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(name_entry), "activate",
			   GTK_SIGNAL_FUNC(useredit_name_activate), ues);
	name_label = gtk_label_new("Name:");
	pass_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(pass_entry), 0);
	gtk_signal_connect(GTK_OBJECT(pass_entry), "activate",
			   GTK_SIGNAL_FUNC(useredit_pass_activate), ues);
	pass_label = gtk_label_new("Pass:");
	gtk_table_set_row_spacings(GTK_TABLE(info_table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(info_table), 5);
	gtk_box_pack_start(GTK_BOX(wvbox), info_frame, 0, 0, 2);

	usermod_scroll = gtk_scrolled_window_new(0, 0);
	SCROLLBAR_SPACING(usermod_scroll) = 0;
	gtk_widget_set_usize(usermod_scroll, 250, 500);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(usermod_scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	avbox = gtk_vbox_new(0, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(usermod_scroll), avbox);
	gtk_box_pack_start(GTK_BOX(wvbox), usermod_scroll, 0, 0, 2);
	gtk_container_add(GTK_CONTAINER(window), wvbox);

	ues->name_entry = name_entry;
	ues->login_entry = login_entry;
	ues->pass_entry = pass_entry;

	gtk_misc_set_alignment(GTK_MISC(login_label), 0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(name_label), 0, 0.5);    
	gtk_misc_set_alignment(GTK_MISC(pass_label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(login_label), GTK_JUSTIFY_LEFT);
	gtk_label_set_justify(GTK_LABEL(name_label), GTK_JUSTIFY_LEFT);               
	gtk_label_set_justify(GTK_LABEL(pass_label), GTK_JUSTIFY_LEFT);
	gtk_table_attach(GTK_TABLE(info_table), login_label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(info_table), login_entry, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(info_table), name_label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(info_table), name_entry, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(info_table), pass_label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(info_table), pass_entry, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	for (i = 0; i < sizeof(access_names)/sizeof(struct access_name); i++) {
		if (access_names[i].bitno == -1) {
			nframes++;
			frame = gtk_frame_new(access_names[i].name);
			vbox = gtk_vbox_new(0, 0);
			gtk_container_add(GTK_CONTAINER(frame), vbox);
			gtk_box_pack_start(GTK_BOX(avbox), frame, 0, 0, 0);
			continue;
		}
		chk = gtk_check_button_new_with_label(access_names[i].name);
		awi = i - nframes;
		ues->access_widgets[awi].bitno = access_names[i].bitno;
		ues->access_widgets[awi].widget = chk;
		gtk_signal_connect(GTK_OBJECT(chk), "clicked",
				   GTK_SIGNAL_FUNC(useredit_chk_activate), ues);
		if (vbox)
			gtk_box_pack_start(GTK_BOX(vbox), chk, 0, 0, 0);
	}

	gtk_widget_show_all(window);
}

void
open_useredit (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	create_useredit_window(ghtlc);
}
