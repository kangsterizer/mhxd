#include "hx.h"
#include "hx_gtk.h"
#include "icons.h"

extern void create_about_window (void);
extern void open_files (gpointer data);
extern void open_users (gpointer data);
extern void open_chat (gpointer data);
extern void open_tracker (gpointer data);
extern void open_useredit (gpointer data);
extern void open_news (gpointer data);
extern void open_tnews (gpointer data);
extern void open_options (gpointer data);
extern void open_connect (gpointer data);
extern void open_tasks (gpointer data);

static void
toolbar_disconnect (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	if (ghtlc->connected)
		hx_htlc_close(ghtlc->htlc);
}

static void
toolbar_close (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	ghtlc_conn_delete(ghtlc);
}

static void
toolbar_destroy (gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;

	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->connectbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->disconnectbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->closebtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->trackerbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->optionsbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->newsbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->tnewsbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->filesbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->usersbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->chatbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->tasksbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->usereditbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->aboutbtn);
	gtk_container_remove(GTK_CONTAINER(ghtlc->toolbar_hbox), ghtlc->quitbtn);
	ghtlc->toolbar_hbox = 0;
}

static void
quit_btn (void)
{
	hx_exit(0);
}

static void
toolbar_buttons_init (struct ghtlc_conn *ghtlc, GtkWidget *window)
{
	GtkWidget *connectbtn;
	GtkWidget *disconnectbtn;
	GtkWidget *closebtn;
	GtkWidget *trackerbtn;
	GtkWidget *optionsbtn;
	GtkWidget *newsbtn;
	GtkWidget *tnewsbtn;
	GtkWidget *filesbtn;
	GtkWidget *usersbtn;
	GtkWidget *chatbtn;
	GtkWidget *tasksbtn;
	GtkWidget *usereditbtn;
	GtkWidget *aboutbtn;
	GtkWidget *quitbtn;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();
	connectbtn = icon_button_new(ICON_CONNECT, "Connect", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(connectbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_connect), (gpointer)ghtlc);
	disconnectbtn = icon_button_new(ICON_KICK, "Disconnect", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(disconnectbtn), "clicked",
				  GTK_SIGNAL_FUNC(toolbar_disconnect), (gpointer)ghtlc);
	closebtn = icon_button_new(ICON_NUKE, "Close", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(closebtn), "clicked",
				  GTK_SIGNAL_FUNC(toolbar_close), (gpointer)ghtlc);
	trackerbtn = icon_button_new(ICON_TRACKER, "Tracker", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(trackerbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_tracker), (gpointer)ghtlc);
	optionsbtn = icon_button_new(ICON_OPTIONS, "Options", window, tooltips); 
	gtk_signal_connect_object(GTK_OBJECT(optionsbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_options), (gpointer)ghtlc);
	newsbtn = icon_button_new(ICON_NEWS, "News", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(newsbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_news), (gpointer)ghtlc);
	tnewsbtn = icon_button_new(ICON_TNEWS, "TNews", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(tnewsbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_tnews), (gpointer)ghtlc);
	filesbtn = icon_button_new(ICON_FILE, "Files", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(filesbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_files), (gpointer)ghtlc);
	usersbtn = icon_button_new(ICON_USER, "Users", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(usersbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_users), (gpointer)ghtlc);
	chatbtn = icon_button_new(ICON_CHAT, "Chat", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(chatbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_chat), (gpointer)ghtlc);
	tasksbtn = icon_button_new(ICON_TASKS, "Tasks", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(tasksbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_tasks), (gpointer)ghtlc);
	usereditbtn = icon_button_new(ICON_YELLOWUSER, "User Edit", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(usereditbtn), "clicked",
				  GTK_SIGNAL_FUNC(open_useredit), (gpointer)ghtlc);
	aboutbtn = icon_button_new(ICON_INFO, "About", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(aboutbtn), "clicked",
				  GTK_SIGNAL_FUNC(create_about_window), 0);
	quitbtn = icon_button_new(ICON_STOP, "Quit", window, tooltips);
	gtk_signal_connect_object(GTK_OBJECT(quitbtn), "clicked",
			  GTK_SIGNAL_FUNC(quit_btn), 0);

	gtk_object_ref(GTK_OBJECT(connectbtn));
	gtk_object_sink(GTK_OBJECT(connectbtn));
	gtk_object_ref(GTK_OBJECT(disconnectbtn));
	gtk_object_sink(GTK_OBJECT(disconnectbtn));
	gtk_object_ref(GTK_OBJECT(closebtn));
	gtk_object_sink(GTK_OBJECT(closebtn));
	gtk_object_ref(GTK_OBJECT(trackerbtn));
	gtk_object_sink(GTK_OBJECT(trackerbtn));
	gtk_object_ref(GTK_OBJECT(optionsbtn));
	gtk_object_sink(GTK_OBJECT(optionsbtn));
	gtk_object_ref(GTK_OBJECT(newsbtn));
	gtk_object_sink(GTK_OBJECT(newsbtn));
	gtk_object_ref(GTK_OBJECT(tnewsbtn));
	gtk_object_sink(GTK_OBJECT(tnewsbtn));
	gtk_object_ref(GTK_OBJECT(filesbtn));
	gtk_object_sink(GTK_OBJECT(filesbtn));
	gtk_object_ref(GTK_OBJECT(usersbtn));
	gtk_object_sink(GTK_OBJECT(usersbtn));
	gtk_object_ref(GTK_OBJECT(chatbtn));
	gtk_object_sink(GTK_OBJECT(chatbtn));
	gtk_object_ref(GTK_OBJECT(tasksbtn));
	gtk_object_sink(GTK_OBJECT(tasksbtn));
	gtk_object_ref(GTK_OBJECT(usereditbtn));
	gtk_object_sink(GTK_OBJECT(usereditbtn));
	gtk_object_ref(GTK_OBJECT(aboutbtn));
	gtk_object_sink(GTK_OBJECT(aboutbtn));
	gtk_object_ref(GTK_OBJECT(quitbtn));
	gtk_object_sink(GTK_OBJECT(quitbtn));

	ghtlc->connectbtn = connectbtn;
	ghtlc->disconnectbtn = disconnectbtn;
	ghtlc->closebtn = closebtn;
	ghtlc->trackerbtn = trackerbtn;
	ghtlc->optionsbtn = optionsbtn;
	ghtlc->newsbtn = newsbtn;
	ghtlc->tnewsbtn = tnewsbtn;
	ghtlc->filesbtn = filesbtn;
	ghtlc->usersbtn = usersbtn;
	ghtlc->chatbtn = chatbtn;
	ghtlc->tasksbtn = tasksbtn;
	ghtlc->usereditbtn = usereditbtn;
	ghtlc->aboutbtn = aboutbtn;
	ghtlc->quitbtn = quitbtn;
}

void
create_toolbar_window (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	struct window_geometry *wg;
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *connectbtn;
	GtkWidget *disconnectbtn;
	GtkWidget *closebtn;
	GtkWidget *trackerbtn;
	GtkWidget *optionsbtn;
	GtkWidget *newsbtn;
	GtkWidget *tnewsbtn;
	GtkWidget *filesbtn;
	GtkWidget *usersbtn;
	GtkWidget *chatbtn;
	GtkWidget *tasksbtn;
	GtkWidget *usereditbtn;
	GtkWidget *aboutbtn;
	GtkWidget *quitbtn;

	if ((gwin = ghx_window_with_wgi(ghtlc, WG_TOOLBAR))) {
		ghx_window_present(gwin);
		return;
	}

	gwin = window_create(ghtlc, WG_TOOLBAR);
	wg = gwin->wg;
	window = gwin->widget;

	gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				  GTK_SIGNAL_FUNC(toolbar_destroy), (gpointer)ghtlc);

	changetitle(ghtlc, window, "Toolbar");

	if (!ghtlc->connectbtn) {
		toolbar_buttons_init(ghtlc, window);
		keyaccel_attach(ghtlc, window);
	}
	connectbtn = ghtlc->connectbtn;
	disconnectbtn = ghtlc->disconnectbtn;
	closebtn = ghtlc->closebtn;
	trackerbtn = ghtlc->trackerbtn;
	optionsbtn = ghtlc->optionsbtn;
	newsbtn = ghtlc->newsbtn;
	tnewsbtn = ghtlc->tnewsbtn;
	filesbtn = ghtlc->filesbtn;
	usersbtn = ghtlc->usersbtn;
	chatbtn = ghtlc->chatbtn;
	tasksbtn = ghtlc->tasksbtn;
	usereditbtn = ghtlc->usereditbtn;
	aboutbtn = ghtlc->aboutbtn;
	quitbtn = ghtlc->quitbtn;

	hbox = gtk_hbox_new(0, 2);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	gtk_container_add(GTK_CONTAINER(window), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), connectbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), disconnectbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), closebtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), trackerbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), optionsbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), newsbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), tnewsbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), filesbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), usersbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), chatbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), tasksbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), usereditbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), aboutbtn, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), quitbtn, 1, 1, 0);

	ghtlc->toolbar_hbox = hbox;

	gtk_widget_show_all(window);

	gtk_widget_set_sensitive(disconnectbtn, ghtlc->connected);
	gtk_widget_set_sensitive(filesbtn, ghtlc->connected);
}
