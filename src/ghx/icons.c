#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "hx.h"
#include "hxd.h"
#include "sys_deps.h"
#include <gtk/gtk.h>
#include "cicn.h"
#include "icons.h"
#include "xmalloc.h"

extern GdkImage *cicn_to_gdkimage (GdkColormap *colormap, GdkVisual *visual,
				   char *cicndata, unsigned int len, GdkImage **maskimp);

struct ifn icon_files;
struct ifn user_icon_files;

#define PTYPE_NORMAL	0
#define PTYPE_AWAY	1

#ifdef CONFIG_DULLED
#define NPTYPES	2
#else
#define NPTYPES 1
#endif
static struct pixmap_cache *pixmap_cache[NPTYPES][256];
static unsigned int pixmap_cache_len[NPTYPES][256];

#define DEFAULT_ICON	128

static struct pixmap_cache *
pixmap_cache_look (u_int16_t icon, struct ifn *ifn, int ptype)
{
	unsigned int len, i = icon % 256;
	struct pixmap_cache *list = pixmap_cache[ptype][i];

	if (list) {
		len = pixmap_cache_len[ptype][i];
		for (i = 0; i < len; i++) {
			if (list[i].icon == icon && list[i].ifn == ifn)
				return &list[i];
		}
	}

	return 0;
}

static struct pixmap_cache *
pixmap_cache_add (u_int16_t icon, GdkPixmap *pixmap, GdkBitmap *mask,
		  int width, int height, int depth, struct ifn *ifn, int ptype)
{
	unsigned int n, i = icon % 256;
	struct pixmap_cache *list = pixmap_cache[ptype][i];

	n = pixmap_cache_len[ptype][i];
	list = realloc(list, (n + 1) * sizeof(struct pixmap_cache));
	if (!list) {
		pixmap_cache[ptype][i] = 0;
		return 0;
	}
	list[n].pixmap = pixmap;
	list[n].mask = mask;
	list[n].ifn = ifn;
	list[n].icon = icon;
	list[n].width = width;
	list[n].height = height;
	list[n].depth = depth;
	pixmap_cache[ptype][i] = list;
	pixmap_cache_len[ptype][i] = n + 1;

	gdk_pixmap_ref(pixmap);
	if (mask)
		gdk_bitmap_ref(mask);

	return &list[n];
}

GdkGC *users_gc;
static GdkGC *mask_gc;

#define TYPE_cicn	0x6369636e

#define default_pixc (load_icon(widget, DEFAULT_ICON, &user_icon_files, browser, 0))

struct pixmap_cache *
load_icon (GtkWidget *widget, u_int16_t icon, struct ifn *ifn, int browser, int ptype)
{
	struct pixmap_cache *pixc;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkGC *gc;
	GdkImage *image;
	GdkImage *maskim;
	GdkVisual *visual;
	GdkColormap *colormap;
	gint off, width, height, depth, x, y;
	macres_res *cicn;
	unsigned int i;

#ifndef CONFIG_DULLED
	ptype = 0;
#endif
	pixc = pixmap_cache_look(icon, ifn, ptype);
	if (pixc)
		return pixc;

	for (i = 0; i < ifn->n; i++) {
		if (!ifn->cicns[i])
			continue;
		cicn = macres_file_get_resid_of_type(ifn->cicns[i], TYPE_cicn, icon);
		if (cicn)
			goto found;
	}
	if (icon == DEFAULT_ICON)
		return 0;
	if (!browser)
		return load_icon(widget, DEFAULT_ICON, &user_icon_files, 0, ptype);
	return 0;
found:
	colormap = gtk_widget_get_colormap(widget);
	visual = gtk_widget_get_visual(widget);
	image = cicn_to_gdkimage(colormap, visual, cicn->data, cicn->datalen, &maskim);
	if (!image)
		return default_pixc;
	depth = image->depth;
	width = image->width;
	height = image->height;
	off = width > 400 ? 198 : 0;

#ifdef CONFIG_DULLED
	if (ptype == PTYPE_AWAY) {
		for (y = 0; y < height; y++) {
			for (x = 0; x < width-off; x++) {
				guint pixel;
				gint r, g, b;
				GdkColor col;

				pixel = gdk_image_get_pixel(image, x, y);
				switch (visual->depth) {
					case 1:
						r = g = b = pixel;
						break;
					case 8:
						r = colormap->colors[pixel].red;
						g = colormap->colors[pixel].green;
						b = colormap->colors[pixel].blue;
						break;
					default:
						r = (pixel & visual->red_mask) >> visual->red_shift;
						g = (pixel & visual->green_mask) >> visual->green_shift;
						b = (pixel & visual->blue_mask) >> visual->blue_shift;
						r = r * (int)(0xff / (visual->red_mask >> visual->red_shift));
						g = g * (int)(0xff / (visual->green_mask >> visual->green_shift));
						b = b * (int)(0xff / (visual->blue_mask >> visual->blue_shift));
						r = (r << 8)|0xff;
						g = (g << 8)|0xff;
						b = (b << 8)|0xff;
						break;
				}

			/*	g = (g + r + b)/6;
				r = b = 0; */
			/*	r = (r * 2) / 3;
				g = (g * 2) / 3;
				b = (b * 2) / 3; */
			/*	r *= 2; if (r > 0xffff) r = 0xffff;
				g *= 2; if (g > 0xffff) g = 0xffff;
				b *= 2; if (b > 0xffff) b = 0xffff; */

				r = (r + 65535) / 2; if (r > 0xffff) r = 0xffff;
				g = (g + 65535) / 2; if (g > 0xffff) g = 0xffff;
				b = (b + 65535) / 2; if (b > 0xffff) b = 0xffff;

				col.pixel = 0;
				col.red = r;
				col.green = g;
				col.blue = b;
				if (!gdk_colormap_alloc_color(colormap, &col, 0, 1)) {
					fprintf(stderr, "rgb_to_pixel: can't allocate %u/%u/%u\n",
						 r, g, b);
					fflush(stderr);
				}
				pixel = col.pixel;
				gdk_image_put_pixel(image, x, y, pixel); 
			} 
		} 
	}
#endif

	pixmap = gdk_pixmap_new(widget->window, width-off, height, depth);
	if (!pixmap)
		return default_pixc;
	gc = users_gc;
	if (!gc) {
		gc = gdk_gc_new(pixmap);
		if (!gc)
			return default_pixc;
		users_gc = gc;
	}
	gdk_draw_image(pixmap, gc, image, off, 0, 0, 0, width-off, height);
	gdk_image_destroy(image);
	if (maskim) {
		mask = gdk_pixmap_new(widget->window, width-off, height, 1);
		if (!mask)
			return default_pixc;
		gc = mask_gc;
		if (!gc) {
			gc = gdk_gc_new(mask);
			if (!gc)
				return default_pixc;
			mask_gc = gc;
		}
		gdk_draw_image(mask, gc, maskim, off, 0, 0, 0, width-off, height);
		gdk_image_destroy(maskim);
	} else {
		mask = 0;
	}

	pixc = pixmap_cache_add(icon, pixmap, mask, width-off, height, depth, ifn, ptype);

	return pixc;
}

static void
init_icons (struct ifn *ifn, unsigned int i)
{
	int fd;

	ifn->cicns = xrealloc(ifn->cicns, sizeof(macres_file *) * ifn->n);
	if (!ifn->files[i])
		goto fark;
	fd = SYS_open(ifn->files[i], O_RDONLY, 0);
	if (fd < 0) {
fark:		ifn->cicns[i] = 0;
		return;
	}
	ifn->cicns[i] = macres_file_open(fd);
}

void
set_icon_files (struct ifn *ifn, const char *str, const char *varstr)
{
	const char *p;
	unsigned int i, j;
	char buf[MAXPATHLEN];

	/* icon_files[*] */
	if (varstr[10] != '[')
		p = &varstr[16];
	else
		p = &varstr[11];
	i = strtoul(p, 0, 0);
	if (i >= ifn->n) {
		ifn->files = xrealloc(ifn->files, sizeof(char *) * (i+1));
		for (j = ifn->n; j < i; j++)
			ifn->files[j] = 0;
		ifn->n = i+1;
	}
	expand_tilde(buf, str);
	ifn->files[i] = xstrdup(buf);
	init_icons(ifn, i);
}

GtkWidget *
icon_pixmap (GtkWidget *widget, u_int16_t icon)
{
	struct pixmap_cache *pixc;
	GtkWidget *gtkpixmap;

	pixc = load_icon(widget, icon, &icon_files, 0, 0);
	if (!pixc)
		return 0;
	gtkpixmap = gtk_pixmap_new(pixc->pixmap, pixc->mask);

	return gtkpixmap;
}

GtkWidget *
icon_button_new (u_int16_t iconid, const char *str, GtkWidget *widget, GtkTooltips *tooltips)
{
	GtkWidget *btn;
	GtkWidget *icon;

	btn = gtk_button_new();
	icon = icon_pixmap(widget, iconid);
	if (icon)
		gtk_container_add(GTK_CONTAINER(btn), icon);
	gtk_tooltips_set_tip(tooltips, btn, str, 0);
	gtk_widget_set_usize(btn, 24, 24);

	return btn;
}
