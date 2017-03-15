#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "xmalloc.h"

static GtkWidget *about_window;

static void
about_close (void)
{
	gtk_widget_destroy(about_window);
}

static void
set_notebook_tab (GtkWidget *notebook, gint page_num, GtkWidget *widget)
{
	GtkNotebookPage *page;
	GtkWidget *notebook_page;

	page = (GtkNotebookPage *)g_list_nth(GTK_NOTEBOOK(notebook)->children, page_num)->data;
	notebook_page = page->child;
	gtk_widget_ref(notebook_page);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_num);
	gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), notebook_page, widget, page_num);
	gtk_widget_unref(notebook_page);
}

#define CREDITS_FONT       "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1"

#define ABOUT_DEFAULT_WIDTH	100
#define ABOUT_MAX_WIDTH		600
#define LINE_SKIP		4

extern const char *hxd_version;

static char Credits[] =
            "Programming: Misha Nasledov\n\
                         <misha@nasledov.com>\n\
                         Ryan Nielsen\n\
                         <ran@fortyoz.org>\n\
                         David Raufeisen\n\
                         <david@fortyoz.org>";

static GdkPixmap *frac_create_logo (GtkWidget *, GdkBitmap **);

void
create_about_window (void)
{
	GtkWidget *fixed;
	GtkWidget *close_button;
	GtkWidget *notebook;
	GtkWidget *fixed1;
	GtkWidget *frame;
	GtkWidget *title_label;
	GtkWidget *scrolledwindow;
	GtkWidget *credits_text;
	GtkWidget *credits_label;
	GtkWidget *pixmap;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GdkFont *credits_font;
	GtkAdjustment *adj;
	char versionstr[64]; 

	about_window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(about_window, 482, 450);
	gtk_window_set_title(GTK_WINDOW(about_window), "About GHx");
	gtk_window_set_policy(GTK_WINDOW(about_window), 0, 0, 0);

	fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(about_window), fixed);

	close_button = gtk_button_new_with_label("Close");
	gtk_fixed_put(GTK_FIXED(fixed), close_button, 384, 393);
	gtk_widget_set_uposition(close_button, 384, 403);
	gtk_widget_set_usize(close_button, 88, 36);
	GTK_WIDGET_SET_FLAGS(close_button, GTK_CAN_DEFAULT);
	gtk_widget_grab_focus(close_button);
	gtk_widget_grab_default(close_button);
	gtk_signal_connect(GTK_OBJECT(close_button), "clicked",
			   GTK_SIGNAL_FUNC(about_close),
			   GTK_OBJECT(about_window));

	gtk_widget_realize(about_window);

	notebook = gtk_notebook_new();
	gtk_fixed_put(GTK_FIXED(fixed), notebook, 8, 8);
	gtk_widget_set_uposition(notebook, 8, 8);
	gtk_widget_set_usize(notebook, 466, 382);
	
	fixed1 = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(notebook), fixed1);

	frame = gtk_frame_new(0);
	gtk_fixed_put(GTK_FIXED(fixed1), frame, 32, 12);
	gtk_widget_set_uposition(frame, 32, 12);
	gtk_widget_set_usize(frame, 400, 200);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

	icon = frac_create_logo(about_window, &mask);
	pixmap = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(frame), pixmap);

	sprintf(versionstr, "GHx %s", hxd_version);
	title_label = gtk_label_new(versionstr);
	gtk_fixed_put(GTK_FIXED(fixed1), title_label, 8, 175);
	gtk_widget_set_uposition(title_label, 8, 215);
	gtk_widget_set_usize(title_label, 448, 16);

	scrolledwindow = gtk_scrolled_window_new(0, 0);
	gtk_fixed_put(GTK_FIXED(fixed1), scrolledwindow, 12, 220);
	gtk_widget_set_uposition(scrolledwindow, 12, 240);
	gtk_widget_set_usize(scrolledwindow, 436, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	credits_font = gdk_font_load(CREDITS_FONT);
	credits_text = gtk_text_new(0, 0);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), credits_text);
	gtk_text_insert(GTK_TEXT(credits_text), credits_font, 0, 0, Credits, -1);
	gtk_text_set_point(GTK_TEXT(credits_text), 0);

	adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolledwindow));
	gtk_adjustment_set_value(adj, 0);

	credits_label = gtk_label_new("Credits");
	set_notebook_tab(notebook, 0, credits_label);
	gtk_widget_show_all(about_window);
}

extern void palette_get_rgb_8 (unsigned int *);

static unsigned int pixel_length[2] = {200, 200};

typedef unsigned int pixel_t;

static pixel_t *pixels;
static pixel_t *pixelsj;
static unsigned int pixels_size;
static pixel_t palette[256];

typedef double real_t;

static real_t start[2];
static real_t end[2];

static unsigned int (*fractal_loop)(real_t, real_t, unsigned int);

static real_t julia_constants[2];

static unsigned int
julia_loop (real_t x, real_t y, unsigned int loop_max)
{
	unsigned int n;
	real_t cx, cy, xsq = 0.0, ysq = 0.0;
	real_t xcb = 0.0, ycb = 0.0;
	real_t x4 = 0.0, y4 = 0.0;
	real_t newx;

	cx = julia_constants[0];
	cy = julia_constants[1];
	for (n = 0; n < loop_max; n++) {
		/* power == 4 */
		xsq = x*x;
		ysq = y*y;
		xcb = xsq*x;
		ycb = ysq*y;
		x4 = xcb*x;
		y4 = ycb*y;
		if (x4 + y4 > 16.0)
			break;
		newx = x4 + y4 - 6.0*xsq*ysq + cx;
		y = 4.0*xcb*y - 4.0*x*ycb + cy;
		x = newx;
	}
	/* if n > palette size or n == 0 */
	if (!(n & 0xff))
		return loop_max-1;

	return n;
}

static unsigned int
mandelbrot_loop (real_t cx, real_t cy, unsigned int loop_max)
{
	unsigned int n;
	real_t x = 0.0, y = 0.0, xsq = 0.0, ysq = 0.0;
	real_t newx;

	for (n = 0; n < loop_max; n++) {
		/* power == 2 */
		newx = xsq - ysq + cx;
		y = 2.0*x*y + cy;
		x = newx;
		xsq = x*x;
		ysq = y*y;
		if (xsq + ysq > 4.0)
			break;
	}
	/* if n > palette size or n == 0 */
	if (!(n & 0xff))
		return loop_max-1;

	return n;
}

static real_t C1[2], C2[2], C3[2];

static unsigned int
mtp (unsigned int d, real_t x)
{
	unsigned int px;

	x += C1[d];
	x *= C2[d];
	x += C3[d];
	px = (unsigned int)x;

	return px;
}

static void
fractal (void)
{
	int loop_max = 256;
	int count;
	pixel_t pix;
	real_t x, y, dx, dy;
	int iy;

	if (fractal_loop == julia_loop) {
		/* fix error (should be 0.01) */
		dx = 0.009;
		dy = 0.009;
	} else {
		dx = 0.000015625;
		dy = 0.000015625;
	}

	for (y = start[1]; y < end[1]; y += dy) {
		iy = (pixel_length[1] - mtp(1, y) - 1) * pixel_length[0];
		for (x = start[0]; x < end[0]; x += dx) {
			count = fractal_loop(x, y, loop_max);
			pix = ~palette[count];
			pixels[iy+mtp(0, x)] = (pix) | 0xff000000;
		}
	}
}

static GdkPixmap *
frac_create_logo (GtkWidget *widget, GdkBitmap **maskp)
{
	GdkWindow *window;
	GdkVisual *visual;
	GdkPixmap *pixmap;
	GdkGC *gc;
	static int init = 0;

	if (!init) {
		unsigned int iy, iyi, y, x;

		init = 1;

		pixels_size = pixel_length[0] * pixel_length[1] * sizeof(pixel_t);
		pixels = xmalloc(pixels_size*2);
		pixelsj = (pixel_t *)(((unsigned char *)pixels) + pixels_size);

		palette_get_rgb_8(palette);

		gdk_rgb_init();

		start[0] = -1.0;
		start[1] = -1.0;
		end[0] = 1.0;
		end[1] = 1.0;
		C1[0] = -(start[0] + ((end[0] - start[0]) * 0.5));
		C2[0] = ((real_t)(pixel_length[0]>>1) / ((end[0] - start[0]) * 0.5));
		C3[0] = pixel_length[0] >> 1;
		C1[1] = -(start[1] + ((end[1] - start[1]) * 0.5));
		C2[1] = ((real_t)(pixel_length[1]>>1) / ((end[1] - start[1]) * 0.5));
		C3[1] = pixel_length[1] >> 1;
		julia_constants[0] = 0.6;
		julia_constants[1] = 0.05;
		fractal_loop = julia_loop;
		fractal();
		memcpy(pixelsj, pixels, pixels_size);

		start[0] = -1.514;
		start[1] = -0.004;
		end[0] = -1.506;
		end[1] = 0.004;
		C1[0] = -(start[0] + ((end[0] - start[0]) * 0.5));
		C2[0] = ((real_t)(pixel_length[0]>>1) / ((end[0] - start[0]) * 0.5));
		C1[1] = -(start[1] + ((end[1] - start[1]) * 0.5));
		C2[1] = ((real_t)(pixel_length[1]>>1) / ((end[1] - start[1]) * 0.5));
		start[1] = 0.000;
		end[0] = -1.510;
		fractal_loop = mandelbrot_loop;
		fractal();
		for (y = 0; y < pixel_length[1]>>1; y++) {
			iy = pixel_length[0] * y;
			for (x = (pixel_length[0]>>1); x < pixel_length[0]; x++) {
				pixels[iy + x] = pixels[iy + (pixel_length[0]-1 - x)];
			}
		}
		for (y = pixel_length[1]>>1; y < pixel_length[1]; y++) {
			iy = pixel_length[0] * y;
			iyi = pixel_length[0] * (pixel_length[1]-1 - y);
			for (x = 0; x < pixel_length[0]; x++) {
				pixels[iy + x] = pixels[iyi + x];
			}
		}
	}

	window = widget->window;
	visual = gtk_widget_get_visual(widget);

	pixmap = gdk_pixmap_new(window, pixel_length[0]*2, pixel_length[1], visual->depth);
	gc = widget->style->fg_gc[GTK_STATE_NORMAL];

	gdk_draw_rgb_32_image(pixmap, gc,
			      0, 0, pixel_length[0], pixel_length[1],
			      GDK_RGB_DITHER_NORMAL, (guchar *)pixels, pixel_length[0]*4);

	gdk_draw_rgb_32_image(pixmap, gc,
			      pixel_length[0], 0, pixel_length[0], pixel_length[1],
			      GDK_RGB_DITHER_NORMAL, (guchar *)pixelsj, pixel_length[0]*4);

#if 0
{
	GdkBitmap *mask;
	GdkImage *maskim;
	GdkGC *maskgc;
	unsigned char *maskfb;

	maskfb = malloc(pixel_length[0]*pixel_length[1]*2/8+1);
	memset(maskfb, 0, pixel_length[0]*pixel_length[1]*2/8+1);
	maskim = gdk_image_new_bitmap(visual, maskfb, pixel_length[0]*2, pixel_length[1]);
	mask = gdk_pixmap_new(window, pixel_length[0]*2, pixel_length[1], 1);
	maskgc = gdk_gc_new(mask);
	gdk_draw_image(mask, maskgc, maskim,
		       0, 0, 0, 0, pixel_length[0]*2, pixel_length[1]);
	gdk_image_destroy(maskim);
	if (maskp)
		*maskp = mask;
}
#else
	if (maskp)
		*maskp = 0;
#endif

	return pixmap;
}
