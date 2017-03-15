#include <gtk/gtk.h>
#include "xmalloc.h"
#include "cmenu.h"

void
context_menu_delete (struct context_menu *cmenu)
{
	struct context_menu_entry *cme = cmenu->entries, *cmeend = cme + cmenu->nentries;

	for (cme = cmenu->entries; cme < cmeend; cme++)
		xfree(cme->name);
	gtk_widget_destroy(cmenu->menu);
	xfree(cmenu);
}

struct context_menu *
context_menu_new (struct context_menu_entry *incme, guint nentries)
{
	struct context_menu_entry *cme, *incmeend = incme + nentries;
	struct context_menu *cmenu;
	GtkWidget *menu, *menuitem;
	guint hid;

	cmenu = xmalloc(sizeof(struct context_menu) + nentries * sizeof(struct context_menu_entry));
	menu = gtk_menu_new();
	cmenu->menu = menu;	
	cmenu->nentries = nentries;
	for (cme = cmenu->entries; incme < incmeend; incme++, cme++) {
		if (incme->name)
			cme->name = xstrdup(incme->name);
		else
			cme->name = 0;
		cme->signal_func = incme->signal_func;
		cme->data = incme->data;
		cme->submenu = incme->submenu;
		if (cme->name)
			menuitem = gtk_menu_item_new_with_label(cme->name);
		else
			menuitem = gtk_menu_item_new();
		if (cme->signal_func)
			hid = gtk_signal_connect_object(GTK_OBJECT(menuitem), "activate",
							cme->signal_func, cme->data);
		else
			hid = 0;
		if (cme->submenu)
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(cme->menuitem), cme->submenu->menu);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		if (!cme->signal_func)
			gtk_widget_set_sensitive(menuitem, 0);
		gtk_widget_show(menuitem);
		cme->menuitem = menuitem;
		cme->hid = hid;
	}

	return cmenu;
}

void
context_menu_set_data (struct context_menu *cmenu, guint i, gpointer data)
{
	struct context_menu_entry *cme = cmenu->entries + i;
	guint hid;
	GtkWidget *menuitem;

	menuitem = cme->menuitem;
	hid = cme->hid;
	if (hid)
		gtk_signal_disconnect(GTK_OBJECT(menuitem), hid);
	if (cme->signal_func)
		hid = gtk_signal_connect_object(GTK_OBJECT(menuitem), "activate",
						cme->signal_func, data);
	cme->hid = hid;
	cme->data = data;
}

void
context_menu_set_submenu (struct context_menu *cmenu, guint i, struct context_menu *submenu)
{
	struct context_menu_entry *cme = cmenu->entries + i;

	cme->submenu = submenu;
	if (submenu) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(cme->menuitem), submenu->menu);
		gtk_widget_set_sensitive(cme->menuitem, 1);
	} else {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(cme->menuitem), 0);
		if (!cme->signal_func)
			gtk_widget_set_sensitive(cme->menuitem, 0);
	}
}
