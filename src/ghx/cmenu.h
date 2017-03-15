#ifndef __ghx_cmenu_h
#define __ghx_cmenu_h

struct context_menu_entry {
	char *name;
	GtkSignalFunc signal_func;
	gpointer data;
	GtkWidget *menuitem;
	struct context_menu *submenu;
	guint hid;
};

struct context_menu {
	GtkWidget *menu;
	guint nentries;
	struct context_menu_entry entries[1];
};

extern void context_menu_delete (struct context_menu *cmenu);
extern struct context_menu *context_menu_new (struct context_menu_entry *incme, guint nentries);
extern void context_menu_set_data (struct context_menu *cmenu, guint i, gpointer data);
extern void context_menu_set_submenu (struct context_menu *cmenu, guint i, struct context_menu *submenu);

#endif /* __ghx_cmenu_h */
