#include "hx.h"
#include "hx_gtk.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <fnmatch.h>
#include <ctype.h>
#include <dirent.h>
#include "xmalloc.h"
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include "gtk_hlist.h"
#include "history.h"
#include "macres.h"
#include "icons.h"
#include "chat.h"
#include "cmenu.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

extern void set_icon_files (struct ifn *ifn, const char *str, const char *varstr);

extern void gfl_delete_all (struct ghtlc_conn *ghtlc);

extern void gchat_create_chat_text (struct gchat *gchat, int first);
extern void create_chat_window (struct ghtlc_conn *ghtlc, struct hx_chat *chat);
extern struct gchat *gchat_with_cid (struct ghtlc_conn *ghtlc, u_int32_t cid);
extern struct gchat *gchat_with_chat (struct ghtlc_conn *ghtlc, struct hx_chat *chat);
extern struct gchat *gchat_new (struct ghtlc_conn *ghtlc, struct hx_chat *chatlist);
extern struct gchat *gchat_with_widget (struct ghtlc_conn *ghtlc, GtkWidget *widget);
extern gint chat_delete_event (gpointer data);

extern void tracker_server_create (struct htlc_conn *htlc,
				   const char *addrstr, u_int16_t port, u_int16_t nusers,
				   const char *nam, const char *desc);
extern void output_file_list (struct htlc_conn *htlc, struct cached_filelist *cfl);
extern void file_info (struct htlc_conn *htlc, const char *icon, const char *type, const char *crea,
			u_int32_t size, const char *name, const char *created, const char *modified,
			const char *comment);
extern void output_file_preview (struct htlc_conn *htlc, const char *filename, const char *type);
extern void file_update (struct htxf_conn *htxf);
extern void task_update (struct htlc_conn *htlc, struct task *tsk);
extern void output_msg (struct htlc_conn *htlc, u_int32_t uid, const char *nam, const char *msgbuf, u_int16_t msglen);
extern void user_delete (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user);
extern void user_change (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
			 const char *nam, u_int16_t icon, u_int16_t color);
extern void users_clear (struct htlc_conn *htlc, struct hx_chat *chat);
extern void user_create (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
			 const char *nam, u_int16_t icon, u_int16_t color);
extern void user_list (struct htlc_conn *htlc, struct hx_chat *chat);
extern void output_agreement (struct htlc_conn *htlc, const char *agreement, u_int16_t len);
extern void output_news_file (struct htlc_conn *htlc, const char *news, u_int16_t len);
extern void output_news_post (struct htlc_conn *htlc, const char *news, u_int16_t len);

extern void open_files (gpointer data);
extern void open_chat (gpointer data);
extern void open_news (gpointer data);
extern void open_users (gpointer data);
extern struct connect_context *create_connect_window (struct ghtlc_conn *ghtlc);
extern void create_toolbar_window (struct ghtlc_conn *ghtlc);
extern void create_tasks_window (struct ghtlc_conn *ghtlc);
extern void create_tracker_window (struct ghtlc_conn *ghtlc);

static struct ghtlc_conn *ghtlc_valid (struct ghtlc_conn *ghtlc);

struct ghtlc_conn *ghtlc_conn_list;

char *def_font = 0;
char *def_users_font = 0;

struct timer {
	struct timer *next, *prev;
	guint id;
	int (*fn)();
	void *ptr;
};

static struct timer *timer_list;

void
timer_add_secs (time_t secs, int (*fn)(), void *ptr)
{
	struct timer *timer;
	guint id;

	id = gtk_timeout_add(secs * 1000, fn, ptr);

	timer = xmalloc(sizeof(struct timer));
	timer->next = 0;
	timer->prev = timer_list;
	if (timer_list)
		timer_list->next = timer;
	timer_list = timer;
	timer->id = id;
	timer->fn = fn;
	timer->ptr = ptr;
}

void
timer_delete_ptr (void *ptr)
{
	struct timer *timer, *prev;

	for (timer = timer_list; timer; timer = prev) {
		prev = timer->prev;
		if (timer->ptr == ptr) {
			if (timer->next)
				timer->next->prev = timer->prev;
			if (timer->prev)
				timer->prev->next = timer->next;
			if (timer == timer_list)
				timer_list = timer->next;
			gtk_timeout_remove(timer->id);
			xfree(timer);
		}
	}
}

static void
hxd_gtk_input (gpointer data, int fd, GdkInputCondition cond)
{
	if (cond == GDK_INPUT_READ) {
		if (hxd_files[fd].ready_read)
			hxd_files[fd].ready_read(fd);
	} else if (cond == GDK_INPUT_WRITE) {
		if (hxd_files[fd].ready_write)
			hxd_files[fd].ready_write(fd);
	}
}

void
hxd_fd_add (int fd)
{
}

void
hxd_fd_del (int fd)
{
}

#if defined(__WIN32__)
#define TAGS_SIZE 4096
#else
#define TAGS_SIZE 1024
#endif

static int rinput_tags[TAGS_SIZE];
static int winput_tags[TAGS_SIZE];

void
hxd_fd_set (int fd, int rw)
{
	int tag;

	if (fd >= TAGS_SIZE) {
		hxd_log("hx_gtk: fd %d >= 1024", fd);
		hx_exit(0);
	}
	if (rw & FDR) {
		if (rinput_tags[fd] != -1)
			return;
		tag = gdk_input_add(fd, GDK_INPUT_READ, hxd_gtk_input, 0);
		rinput_tags[fd] = tag;
	}
	if (rw & FDW) {
		if (winput_tags[fd] != -1)
			return;
		tag = gdk_input_add(fd, GDK_INPUT_WRITE, hxd_gtk_input, 0);
		winput_tags[fd] = tag;
	}
}

void
hxd_fd_clr (int fd, int rw)
{
	int tag;

	if (fd >= TAGS_SIZE) {
		hxd_log("hx_gtk: fd %d >= %d", fd, TAGS_SIZE);
		hx_exit(0);
	}
	if (rw & FDR) {
		tag = rinput_tags[fd];
		gdk_input_remove(tag);
		rinput_tags[fd] = -1;
	}
	if (rw & FDW) {
		tag = winput_tags[fd];
		gdk_input_remove(tag);
		winput_tags[fd] = -1;
	}
}

GdkColor colors[16];
GdkColor gdk_user_colors[4];

GdkColor *
colorgdk (u_int16_t color)
{
	return &gdk_user_colors[color % 4];
}

#if defined(GTK_USES_X11)

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

/* From GTK+ 2.2.1 */
/**
 * gdk_window_focus:
 * @window: a #GdkWindow
 * @timestamp: timestamp of the event triggering the window focus
 *
 * Sets keyboard focus to @window. If @window is not onscreen this
 * will not work. In most cases, gtk_window_present() should be used on
 * a #GtkWindow, rather than calling this function.
 * 
 **/
void
my_gdk_window_focus (GdkWindow *window,
		     guint32    timestamp)
{
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.message_type = XInternAtom(GDK_WINDOW_XDISPLAY(window), "_NET_ACTIVE_WINDOW", FALSE);
      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 0;
      
      XSendEvent (GDK_WINDOW_XDISPLAY(window), GDK_ROOT_WINDOW(), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
    }
    {
      XRaiseWindow (GDK_WINDOW_XDISPLAY(window), GDK_WINDOW_XWINDOW(window));

      /* There is no way of knowing reliably whether we are viewable so we need
       * to trap errors so we don't cause a BadMatch.
       */
      gdk_error_trap_push ();
      XSetInputFocus (GDK_WINDOW_XDISPLAY(window),
                      GDK_WINDOW_XWINDOW(window),
                      RevertToParent,
                      timestamp);
      XSync (GDK_WINDOW_XDISPLAY(window), False);
      gdk_error_trap_pop ();
    }
}
#else
void
my_gdk_window_focus (GdkWindow *window,
		     guint32    timestamp)
{
}
#endif

void
window_present (GtkWidget *win)
{
	/* gtk_window_present is a GTK2 function */
	/* gtk_window_present(GTK_WINDOW(win)); */
	if (GTK_WIDGET_VISIBLE(win)) {
		gdk_window_show(win->window);
		/* gdk_window_focus(win->window, gtk_get_current_event_time()); */
		my_gdk_window_focus(win->window, 0);
	} else {
		gtk_widget_show(win);
	}
}

void
ghx_window_present (struct ghx_window *gwin)
{
	window_present(gwin->widget);
}

struct connect_context;

static gint
key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	struct ghtlc_conn *ghtlc = (struct ghtlc_conn *)data;
	struct gchat *gchat;
	guint k;

	if (!ghtlc_valid(ghtlc))
		return 0;

	k = event->keyval;
	/* MOD1 == ALT */
	if ((event->state & GDK_CONTROL_MASK) || (event->state & GDK_MOD1_MASK)) {
		switch (k) {
			case 'b':
				create_toolbar_window(ghtlc);
				break;
			case 'f':
				open_files(data);
				break;
			case 'h':
				open_chat(data);
				break;
			case 'k':
				create_connect_window(ghtlc);
				break;
			case 'n':
				open_news(data);
				break;
			case 'q':
				hx_exit(0);
				break;
			case 'r':
				create_tracker_window(ghtlc);
				break;
			case 't':
				create_tasks_window(ghtlc);
				break;
			case 'u':
				if (!(event->state & GDK_CONTROL_MASK))
					open_users(data);
				break;
			case 'w':
				gchat = gchat_with_widget(ghtlc, widget);
				if (gchat) {
					int destroy;
					if (!(gchat->chat && gchat->chat->cid))
						destroy = 1;
					else
						destroy = 0;
					chat_delete_event(gchat);
					if (!destroy)
						break;
				}
				gtk_widget_destroy(widget);
				break;
			default:
				return 0;
		}
	}

	return 1;
}

void
keyaccel_attach (struct ghtlc_conn *ghtlc, GtkWidget *widget)
{
	gtk_signal_connect(GTK_OBJECT(widget), "key_press_event",
			   GTK_SIGNAL_FUNC(key_press), ghtlc);
}

static struct window_geometry default_window_geometry[NWG];

static struct window_geometry *
wg_get (unsigned int i)
{
	return &default_window_geometry[i];
}

#if 0
static struct ghx_window *
ghx_window_with_widget (struct ghtlc_conn *ghtlc, GtkWidget *widget)
{
	struct ghx_window *gwin;

	for (gwin = ghtlc->window_list; gwin; gwin = gwin->prev) {
		if (gwin->widget == widget)
			return gwin;
	}

	return 0;
}
#endif

static gint
window_configure_event (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	struct ghx_window *gwin = (struct ghx_window *)data;
	struct window_geometry *wg = gwin->wg;

	if (event->send_event) {
		wg->xpos = event->x;
		wg->ypos = event->y;
	} else {
		wg->width = event->width;
		wg->height = event->height;
		wg->xoff = event->x;
		wg->yoff = event->y;
	}

	default_window_geometry[gwin->wgi] = *wg;

	return 1;
}

static void
window_delete_all (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin, *prev;

	for (gwin = ghtlc->window_list; gwin; gwin = prev) {
		prev = gwin->prev;
		gtk_widget_destroy(gwin->widget);
	}
}

void ghtlc_conn_delete (struct ghtlc_conn *ghtlc);

static void
window_delete (struct ghx_window *gwin)
{
	struct ghtlc_conn *ghtlc = gwin->ghtlc;

	if (gwin->next)
		gwin->next->prev = gwin->prev;
	if (gwin->prev)
		gwin->prev->next = gwin->next;
	if (gwin == ghtlc->window_list)
		ghtlc->window_list = gwin->prev;
	xfree(gwin);
	if (!ghtlc->window_list)
		ghtlc_conn_delete(ghtlc);
}

static void
window_destroy (GtkWidget *widget, gpointer data)
{
	struct ghx_window *gwin = (struct ghx_window *)data;

	window_delete(gwin);
}

struct ghx_window *
window_create (struct ghtlc_conn *ghtlc, unsigned int wgi)
{
	GtkWidget *window;
	struct ghx_window *gwin;
	struct window_geometry *wg;

	wg = wg_get(wgi);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	if (!window)
		return 0;

	gwin = xmalloc(sizeof(struct ghx_window));
	gwin->next = 0;
	gwin->prev = ghtlc->window_list;
	if (ghtlc->window_list)
		ghtlc->window_list->next = gwin;
	ghtlc->window_list = gwin;
	gwin->ghtlc = ghtlc;
	gwin->wgi = wgi;
	gwin->wg = wg;
	gwin->widget = window;

	gtk_window_set_policy(GTK_WINDOW(window), 1, 1, 0);
	if (wg->width || wg->height)
		gtk_widget_set_usize(window, wg->width, wg->height);
	gtk_widget_set_uposition(window, wg->xpos-wg->xoff, wg->ypos-wg->yoff);
	gtk_signal_connect(GTK_OBJECT(window), "configure_event",
			   GTK_SIGNAL_FUNC(window_configure_event), gwin);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(window_destroy), gwin);
	keyaccel_attach(ghtlc, window);

	return gwin;
}

struct ghx_window *
ghx_window_with_wgi (struct ghtlc_conn *ghtlc, unsigned int wgi)
{
	struct ghx_window *gwin;

	for (gwin = ghtlc->window_list; gwin; gwin = gwin->prev) {
		if (gwin->wgi == wgi)
			return gwin;
	}

	return 0;
}

static void
setbtns (struct ghtlc_conn *ghtlc, int on)
{
	if (ghtlc->user_msgbtn) {
		gtk_widget_set_sensitive(ghtlc->user_msgbtn, on);
		gtk_widget_set_sensitive(ghtlc->user_infobtn, on);
		gtk_widget_set_sensitive(ghtlc->user_kickbtn, on);
		gtk_widget_set_sensitive(ghtlc->user_banbtn, on);
		gtk_widget_set_sensitive(ghtlc->user_chatbtn, on);
	}
	if (ghtlc->news_postbtn) {
		gtk_widget_set_sensitive(ghtlc->news_postbtn, on);
		gtk_widget_set_sensitive(ghtlc->news_reloadbtn, on);
	}
	if (ghtlc->disconnectbtn) {
		gtk_widget_set_sensitive(ghtlc->disconnectbtn, on);
		gtk_widget_set_sensitive(ghtlc->filesbtn, on);
	}
}

void
changetitle (struct ghtlc_conn *ghtlc, GtkWidget *window, char *nam)
{
	char title[32];
	char addr[HOSTLEN+1];

	inaddr2str(addr, &ghtlc->htlc->sockaddr);
	if (ghtlc->connected) {
		sprintf(title, "%s (%s)", nam, addr);
		gtk_window_set_title(GTK_WINDOW(window), title);
	} else
		gtk_window_set_title(GTK_WINDOW(window), nam);
}

static void
changetitlesconnected (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	char title[32];
	char addr[HOSTLEN+1];
	
	inaddr2str(addr, &ghtlc->htlc->sockaddr);
	for (gwin = ghtlc->window_list; gwin; gwin = gwin->prev) {
		switch (gwin->wgi) {
			case WG_CHAT:
				sprintf(title, "Chat (%s)", addr);
				break;
			case WG_TOOLBAR:
				sprintf(title, "Toolbar (%s)", addr);
				break;
			case WG_NEWS:
				sprintf(title, "News (%s)", addr);
				break;
			case WG_TASKS:
				sprintf(title, "Tasks (%s)", addr);
				break;
			case WG_USERS:
				sprintf(title, "Users (%s)", addr);
				break;
			default:
				continue;
		}
		gtk_window_set_title(GTK_WINDOW(gwin->widget), title);
	}
}

static void
changetitlesdisconnected (struct ghtlc_conn *ghtlc)
{
	struct ghx_window *gwin;
	char *title;

	for (gwin = ghtlc->window_list; gwin; gwin = gwin->prev) {
		switch (gwin->wgi) {
			case WG_CHAT:
				title = "Chat";
				break;
			case WG_TOOLBAR:
				title = "Toolbar";
				break;
			case WG_NEWS:
				title = "News";
				break;
			case WG_TASKS:
				title = "Tasks";
				break;
			case WG_USERS:
				title = "Users";
				break;
			default:
				continue;
		}
		gtk_window_set_title(GTK_WINDOW(gwin->widget), title);
	}
}

struct ghtlc_conn *
ghtlc_conn_new (struct htlc_conn *htlc)
{
	struct ghtlc_conn *ghtlc;

	ghtlc = xmalloc(sizeof(struct ghtlc_conn));
	memset(ghtlc, 0, sizeof(struct ghtlc_conn));
	ghtlc->next = 0;
	ghtlc->prev = ghtlc_conn_list;
	if (ghtlc_conn_list)
		ghtlc_conn_list->next = ghtlc;
	ghtlc->htlc = htlc;
	if (htlc->fd)
		ghtlc->connected = 1;
	else
		ghtlc->connected = 0;
	ghtlc_conn_list = ghtlc;

	return ghtlc;
}

void
ghtlc_conn_delete (struct ghtlc_conn *ghtlc)
{
	if (!ghtlc->window_list)
		return;
	if (ghtlc->connected)
		hx_htlc_close(ghtlc->htlc);
	if (ghtlc->gfile_list)
		gfl_delete_all(ghtlc);
	if (ghtlc->window_list)
		window_delete_all(ghtlc);
	if (ghtlc->next)
		ghtlc->next->prev = ghtlc->prev;
	if (ghtlc->prev)
		ghtlc->prev->next = ghtlc->next;
	if (ghtlc == ghtlc_conn_list)
		ghtlc_conn_list = ghtlc->prev;
	if (ghtlc->htlc != &hx_htlc)
		xfree(ghtlc->htlc);
	xfree(ghtlc);
	if (!ghtlc_conn_list)
		hx_exit(0);
}

static struct ghtlc_conn *
ghtlc_valid (struct ghtlc_conn *ghtlc)
{
	struct ghtlc_conn *ghtlcp;

	for (ghtlcp = ghtlc_conn_list; ghtlcp; ghtlcp = ghtlcp->prev) {
		if (ghtlcp == ghtlc)
			return ghtlc;
	}

	return 0;
}

struct ghtlc_conn *
ghtlc_conn_with_htlc (struct htlc_conn *htlc)
{
	struct ghtlc_conn *ghtlc;

	for (ghtlc = ghtlc_conn_list; ghtlc; ghtlc = ghtlc->prev) {
		if (ghtlc->htlc == htlc)
			return ghtlc;
	}

	return 0;
}

static void
ghtlc_conn_connect (struct ghtlc_conn *ghtlc)
{
	ghtlc->connected = 1;
	setbtns(ghtlc, 1);
	changetitlesconnected(ghtlc);
}

static void
ghtlc_conn_disconnect (struct ghtlc_conn *ghtlc)
{
	ghtlc->connected = 0;
	setbtns(ghtlc, 0);
	changetitlesdisconnected(ghtlc);
}

char *
colorstr (u_int16_t color)
{
	char *col;

	col = g_user_colors[color % 4];

	return col;
}

static void
fe_init (void)
{
	struct ghtlc_conn *ghtlc;

	ghtlc = ghtlc_conn_with_htlc(&hx_htlc);
	create_toolbar_window(ghtlc);
	create_chat_window(ghtlc, 0);
}

static void
loop (void)
{
	fe_init();
	gtk_main();
}

static void
set_window_geometry (struct window_geometry *wg, const char *str, const char *varstr)
{
	const char *p, *strp;
	char buf[8];
	unsigned int i, w, h, x, y, len;

	/* "window_geometry[*][*]" */
	p = &varstr[16];
	if (!strncmp(p, "0]", 2) || !strncmp(p, "default]", 8)) {
		wg = default_window_geometry;
	} else {
		return;
	}
	p = strchr(p, ']');
	if (!p)
		return;
	p += 2;
	if (isdigit(*p)) {
		i = strtoul(p, 0, 0);
	} else if (!strncmp(p, "chat]", 5)) {
		i = WG_CHAT;
	} else if (!strncmp(p, "toolbar]", 8)) {
		i = WG_TOOLBAR;
	} else if (!strncmp(p, "tasks]", 6)) {
		i = WG_TASKS;
	} else if (!strncmp(p, "news]", 5)) {
		i = WG_NEWS;
	} else if (!strncmp(p, "post]", 5)) {
		i = WG_POST;
	} else if (!strncmp(p, "users]", 5)) {
		i = WG_USERS;
	} else if (!strncmp(p, "tracker]", 8)) {
		i = WG_TRACKER;
	} else if (!strncmp(p, "options]", 8)) {
		i = WG_OPTIONS;
	} else if (!strncmp(p, "useredit]", 9)) {
		i = WG_USEREDIT;
	} else if (!strncmp(p, "connect]", 8)) {
		i = WG_CONNECT;
	} else if (!strncmp(p, "files]", 6)) {
		i = WG_FILES;
	} else if (!strncmp(p, "preview]", 8)) {
		i = WG_PREVIEW;
	} else {
		return;
	}

	w = h = x = y = 0;
	for (p = strp = str; ; p++) {
		if (*p == 'x' || *p == '+' || *p == '-' || *p == 0) {
			len = (unsigned)(p - strp) >= sizeof(buf) ? sizeof(buf)-1 : (unsigned)(p - strp);
			if (!len)
				break;
			memcpy(buf, strp, len);
			buf[len] = 0;
			if (*p == 'x')
				w = strtoul(buf, 0, 0);
			else if (*p == 0 || *p == '+' || *p == '-') {
				if (!h)
					h = strtoul(buf, 0, 0);
				else if (!x)
					x = strtoul(buf, 0, 0);
				else
					y = strtoul(buf, 0, 0);
			}
			strp = p + 1;
		}
		if (*p == 0)
			break;
	}

	if (!w || !h)
		return;
	wg[i].width = w;
	wg[i].height = h;
	wg[i].xpos = x;
	wg[i].ypos = y;
}

static void
change_chat_font (struct ghtlc_conn *ghtlc, GdkFont *font)
{
	struct gchat *gchat;
	GtkStyle *style;
	char *in_chars, *out_chars, *subj_text;
	guint in_len, out_len;

	ghtlc->chat_font = font;
	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		style = gtk_widget_get_style(gchat->chat_output_text);
		gdk_font_ref(font);
		style->font = font;
		in_len = gtk_text_get_length(GTK_TEXT(gchat->chat_input_text));
		out_len = gtk_text_get_length(GTK_TEXT(gchat->chat_output_text));
		in_chars = gtk_editable_get_chars(GTK_EDITABLE(gchat->chat_input_text), 0, in_len);
		out_chars = gtk_editable_get_chars(GTK_EDITABLE(gchat->chat_output_text), 0, out_len);
		subj_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(gchat->subject_entry)));
		gtk_widget_destroy(gchat->chat_input_text);
		gtk_widget_destroy(gchat->chat_output_text);
		gtk_widget_destroy(gchat->chat_vscrollbar);
		gtk_widget_destroy(gchat->subject_entry);
		gchat_create_chat_text(gchat, 0);
		if (gchat->chat_in_hbox) {
			gtk_box_pack_start(GTK_BOX(gchat->chat_in_hbox),
					   gchat->chat_input_text, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(gchat->chat_out_hbox),
					   gchat->chat_output_text, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(gchat->chat_out_hbox),
					   gchat->chat_vscrollbar, 0, 0, 0);
			gtk_box_pack_start(GTK_BOX(gchat->subject_hbox),
					   gchat->subject_entry, 1, 1, 0);
		}
		gtk_text_insert(GTK_TEXT(gchat->chat_input_text), 0, 0, 0, in_chars, in_len);
		gtk_text_insert(GTK_TEXT(gchat->chat_output_text), 0, 0, 0, out_chars, out_len);
		gtk_entry_set_text(GTK_ENTRY(gchat->subject_entry), subj_text);
		gtk_widget_set_style(gchat->chat_input_text, style);
		gtk_widget_set_style(gchat->chat_output_text, style);
		gtk_widget_set_style(gchat->subject_entry, style);
		gtk_widget_show(gchat->chat_input_text);
		gtk_widget_show(gchat->chat_output_text);
		gtk_widget_show(gchat->chat_vscrollbar);
		gtk_widget_show(gchat->subject_entry);
		g_free(in_chars);
		g_free(out_chars);
		g_free(subj_text);
	}
}

static void
change_users_font (struct ghtlc_conn *ghtlc, GdkFont *font)
{
	struct gchat *gchat;
	GtkStyle *style;

	style = ghtlc->users_style;
	if (!style)
		return;
	style->font = font;
	gdk_font_ref(font);
	for (gchat = ghtlc->gchat_list; gchat; gchat = gchat->prev) {
		/* style = gtk_widget_get_style(gchat->users_list)
		   gdk_font_ref(font);
		   style->font = font; */
		gtk_widget_set_style(gchat->users_list, style);
	}
}

static void
set_font (char **fontstrp, const char *str, const char *varstr)
{
	struct ghtlc_conn *ghtlc;
	GdkFont *font;
	char *fontstr;

	if (!fontstrp)
		return;
	fontstr = *fontstrp;
	if (fontstr) {
		if (!strcmp(fontstr, str))
			return;
		xfree(fontstr);
	}
	fontstr = xstrdup(str);
	*fontstrp = fontstr;
	font = gdk_fontset_load(fontstr);
	if (font) {
		if (varstr[0] == 'c') { /* chat font */
			for (ghtlc = ghtlc_conn_list; ghtlc; ghtlc = ghtlc->prev)
				change_chat_font(ghtlc, font);
		} else if (varstr[0] == 'u') { /* users font */
			for (ghtlc = ghtlc_conn_list; ghtlc; ghtlc = ghtlc->prev)
				change_users_font(ghtlc, font);
		}
	}
}

static void
init (int argc, char **argv)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	unsigned int i;

	variable_add(default_window_geometry, set_window_geometry,
		     "window_geometry\\[*\\]\\[*\\]");
	variable_add(&icon_files, set_icon_files,
		     "icon_files\\[*\\]");
	variable_add(&user_icon_files, set_icon_files,
		     "user_icon_files\\[*\\]");
	def_font = xstrdup("fixed");
	def_users_font = xstrdup("-adobe-helvetica-bold-r-normal-*-12-*-*-*-p-*-iso8859-1");
	variable_add(&def_font, set_font,
		     "chat_font\\[*\\]\\[*\\]");
	variable_add(&def_users_font, set_font,
		     "users_font\\[*\\]\\[*\\]");

	for (i = 0; i < TAGS_SIZE; i++) {
		rinput_tags[i] = -1;
		winput_tags[i] = -1;
	}

#if defined(CONFIG_ICONV)
	gtk_set_locale();
#endif
	gtk_init(&argc, &argv);

	ghtlc = ghtlc_conn_new(&hx_htlc);
	gchat = gchat_new(ghtlc, hx_htlc.chat_list);
	gchat_create_chat_text(gchat, 1);
}

static void
chat_output (struct gchat *gchat, const char *buf, size_t len)
{
	GtkAdjustment *adj;
	GtkWidget *text;
	guint val, scroll;
	const char *p, *realtext, *end;
	char const *bufp;
	char numstr[4];
	GdkColor *bgcolor, *fgcolor;
	int i, bold = 0;

	text = gchat->chat_output_text;
	if (!text)
		return;
	adj = (GTK_TEXT(text))->vadj;
	val = adj->upper - adj->lower - adj->page_size;
	scroll = adj->value == val;
	gtk_text_freeze(GTK_TEXT(text));
	fgcolor = &text->style->fg[GTK_STATE_NORMAL];
	bgcolor = &text->style->bg[GTK_STATE_NORMAL];
	if (gchat->do_lf)
		gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, "\n", 1);
	bufp = buf;
	end = bufp + len;
	gchat->do_lf = (*(end-1) == '\n');
#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len, in_len;

	in_len = len;
	if (gchat->do_lf)
		in_len--;
	/* XXX Why do we have to do this? */
	out_len = convbuf(g_encoding, g_encoding, buf, in_len, &out_p);
	if (!out_len)
		goto thaw;
	end = out_p+out_len;
	bufp = out_p;
}
#endif
	realtext = bufp;
	for (p = bufp; p < end; p++) {
		if (*p == '\033' && *(p+1) == '[') {
			gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, realtext, p - realtext);
			p += 2;
			i = 0;
			while (i < 2 && ((*p >= '0' && *p <= '7') || *p == ';')) {
				if (*p == ';') {
					if (i)
						bold = numstr[0] == '1' ? 8 : 0;
					i = 0;
					p++;
					continue;
				}
				numstr[i++] = *p;
				p++;
			}
			realtext = p+1;
			if (i) {
				numstr[i--] = 0;
				i = i == 1 ? numstr[i] - '0' : 0;
				i += 8 * (numstr[0] - '0');
				if (!i) {
					fgcolor = &text->style->fg[GTK_STATE_NORMAL];
					bgcolor = &text->style->bg[GTK_STATE_NORMAL];
					continue;
				}
				i &= 0xf;
				if (i >= 8)
					fgcolor = &colors[i-8+bold];
				else
					bgcolor = &colors[i+bold];
			}
		}
	}
	gtk_text_insert(GTK_TEXT(text), 0, fgcolor, bgcolor, realtext, p - realtext - (*(p-1)=='\n'?1:0));
#if defined(CONFIG_ICONV)
thaw:
#endif
	gtk_text_thaw(GTK_TEXT(text));
	if (scroll)
		gtk_adjustment_set_value(adj, adj->upper - adj->lower - adj->page_size);
}

void
hx_printf (struct htlc_conn *htlc, struct hx_chat *chat, const char *fmt, ...)
{
	va_list ap;
	va_list save;
	char autobuf[1024], *buf;
	size_t mal_len;
	int len;
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	__va_copy(save, ap);
	mal_len = sizeof(autobuf);
	buf = autobuf;
	for (;;) {
		va_start(ap, fmt);
		len = vsnprintf(buf, mal_len, fmt, ap);
		va_end(ap);
		if (len != -1)
			break;
		__va_copy(ap, save);
		mal_len <<= 1;
		if (buf == autobuf)
			buf = xmalloc(mal_len);
		else
			buf = xrealloc(buf, mal_len);
	}

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (ghtlc) {
		gchat = gchat_with_chat(ghtlc, chat);
		if (!gchat)
			gchat = gchat_with_cid(ghtlc, 0);
		chat_output(gchat, buf, len);
	}

	if (buf != autobuf)
		xfree(buf);
}

void
hx_printf_prefix (struct htlc_conn *htlc, struct hx_chat *chat, const char *prefix, const char *fmt, ...)
{
	va_list ap;
	va_list save;
	char autobuf[1024], *buf;
	size_t mal_len;
	int len;
	size_t plen;
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	__va_copy(save, ap);
	mal_len = sizeof(autobuf);
	buf = autobuf;
	if (prefix)
		plen = strlen(prefix);
	else
		plen = 0;
	for (;;) {
		va_start(ap, fmt);
		len = vsnprintf(buf + plen, mal_len - plen, fmt, ap);
		va_end(ap);
		if (len != -1)
			break;
		__va_copy(ap, save);
		mal_len <<= 1;
		if (buf == autobuf)
			buf = xmalloc(mal_len);
		else
			buf = xrealloc(buf, mal_len);
	}
	memcpy(buf, prefix, plen);

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (ghtlc) {
		gchat = gchat_with_chat(ghtlc, chat);
		if (!gchat)
			gchat = gchat_with_cid(ghtlc, 0);
		chat_output(gchat, buf, len+plen);
	}

	if (buf != autobuf)
		xfree(buf);
}

static void
output_user_info (struct htlc_conn *htlc, u_int32_t uid, const char *nam,
		  const char *info, u_int16_t len)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *info_window;
	GtkWidget *info_text;
	GtkStyle *style;
	char infotitle[64];

	ghtlc = ghtlc_conn_with_htlc(htlc);
	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(info_window), 1, 1, 0);
	gtk_widget_set_usize(info_window, 256, 256);

	sprintf(infotitle, "User Info: %s (%u)", nam, uid);
	gtk_window_set_title(GTK_WINDOW(info_window), infotitle); 

	info_text = gtk_text_new(0, 0);
	if (ghtlc->gchat_list) {
		style = gtk_widget_get_style(ghtlc->gchat_list->chat_output_text);
		gtk_widget_set_style(info_text, style);
	}
#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	/* XXX Why do we have to do this? */
	out_len = convbuf(g_encoding, g_encoding, info, len, &out_p);
	if (out_len)
		gtk_text_insert(GTK_TEXT(info_text), 0, 0, 0, out_p, out_len);
}
#else
	gtk_text_insert(GTK_TEXT(info_text), 0, 0, 0, info, len);
#endif
	gtk_container_add(GTK_CONTAINER(info_window), info_text);
	gtk_widget_show_all(info_window);
	keyaccel_attach(ghtlc, info_window);
}

void
hx_save (struct htlc_conn *htlc, struct hx_chat *chat, const char *filnam)
{
	struct ghtlc_conn *ghtlc;
	GtkWidget *text;
	char *chars;
	int f;
	ssize_t r;
	size_t pos, len;
	char path[MAXPATHLEN];

	expand_tilde(path, filnam);
	f = SYS_open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (f < 0) {
		hx_printf(htlc, chat, "save: %s: %s\n", path, strerror(errno));
		return;
	}
	pos = 0;
	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc || !ghtlc->gchat_list)
		return;
	text = ghtlc->gchat_list->chat_output_text;
	len = gtk_text_get_length(GTK_TEXT(text));
	chars = gtk_editable_get_chars(GTK_EDITABLE(text), 0, len);
	while (len) {
		r = write(f, chars + pos, len);
		if (r <= 0)
			break;
		pos += r;
		len -= r;
	}
	SYS_fsync(f);
	close(f);
	g_free(chars);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%d bytes written to %s\n", pos, path);
}

static void
wg_save (void)
{
	struct ghtlc_conn *ghtlc;
	struct window_geometry *wg;
	char buf[32];
	unsigned int i;

	ghtlc = ghtlc_conn_with_htlc(&hx_htlc);
	if (!ghtlc)
		return;
	for (i = 0; i < NWG; i++) {
		wg = wg_get(i);
		snprintf(buf, sizeof(buf), "%ux%u%c%d%c%d", wg->width, wg->height,
			 (wg->xpos-wg->xoff) < 0 ? '-' : '+', abs(wg->xpos-wg->xoff),
			 (wg->ypos-wg->yoff) < 0 ? '-' : '+', abs(wg->ypos-wg->yoff));
		switch (i) {
			case WG_CHAT:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][chat]", buf);
				break;
			case WG_TOOLBAR:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][toolbar]", buf);
				break;
			case WG_USERS:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][users]", buf);
				break;
			case WG_TASKS:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][tasks]", buf);
				break;
			case WG_NEWS:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][news]", buf);
				break;
			case WG_POST:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][post]", buf);
				break;
			case WG_TRACKER:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][tracker]", buf);
				break;
			case WG_OPTIONS:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][options]", buf);
				break;
			case WG_USEREDIT:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][useredit]", buf);
				break;
			case WG_CONNECT:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][connect]", buf);
				break;
			case WG_FILES:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][files]", buf);
			case WG_PREVIEW:
				variable_set(ghtlc->htlc, 0, "window_geometry[0][preview]", buf);
				break;
		}
	}
	hx_savevars();
}

static void
cleanup (void)
{
	wg_save();
	gtk_main_quit();
}

static void
status ()
{
}

static void
clear (struct htlc_conn *htlc, struct hx_chat *chat)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;
	GtkWidget *text;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		return;
	gchat = gchat_with_chat(ghtlc, chat);
	if (!gchat || !gchat->chat_output_text)
		return;
	text = gchat->chat_output_text;
	gtk_text_set_point(GTK_TEXT(text), 0);
	gtk_text_forward_delete(GTK_TEXT(text), gtk_text_get_length(GTK_TEXT(text)));
}

static void
term_mode_underline ()
{
}

static void
term_mode_clear ()
{
}

static void
output_chat (struct htlc_conn *htlc, u_int32_t cid, char *chat, u_int16_t chatlen)
{
	struct ghtlc_conn *ghtlc;
	struct gchat *gchat;

	chat[chatlen] = '\n';
	ghtlc = ghtlc_conn_with_htlc(htlc);
	gchat = gchat_with_cid(ghtlc, cid);
	if (gchat)
		chat_output(gchat, chat, chatlen+1);
}

static void
on_connect (struct htlc_conn *htlc)
{
	struct ghtlc_conn *ghtlc;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (!ghtlc)
		ghtlc = ghtlc_conn_new(htlc);
	if (ghtlc)
		ghtlc_conn_connect(ghtlc);

	hx_get_user_list(htlc, 0);
	if (ghx_window_with_wgi(ghtlc, WG_NEWS))
		hx_get_news(htlc);
}

static void
on_disconnect (struct htlc_conn *htlc)
{
	struct ghtlc_conn *ghtlc;

	ghtlc = ghtlc_conn_with_htlc(htlc);
	if (ghtlc)
		ghtlc_conn_disconnect(ghtlc);
}

extern void output_news_dirlist (struct htlc_conn *htlc, struct news_folder *news);
extern void output_news_catlist (struct htlc_conn *htlc, struct news_group *group);
extern void output_news_thread (struct htlc_conn *htlc, struct news_post *post);

struct output_functions hx_gtk_output = {
	init,
	loop,
	cleanup,
	status,
	clear,
	term_mode_underline,
	term_mode_clear,
	output_chat,
	chat_subject,
	chat_password,
	chat_invite,
	chat_delete,
	output_msg,
	output_agreement,
	output_news_file,
	output_news_post,
	output_news_dirlist,
	output_news_catlist,
	output_news_thread,
	output_user_info,
	user_create,
	user_delete,
	user_change,
	user_list,
	users_clear,
	output_file_list,
	file_info,
	output_file_preview,
	file_update,
	tracker_server_create,
	task_update,
	on_connect,
	on_disconnect
};
