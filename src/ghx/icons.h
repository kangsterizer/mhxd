#ifndef __ghx_icons_h
#define __ghx_icons_h

#include "macres.h"

struct ifn {
	char **files;
	macres_file **cicns;
	unsigned int n;
};

extern struct ifn icon_files;
extern struct ifn user_icon_files;

struct pixmap_cache {
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	struct ifn *ifn;
	u_int16_t icon;
	u_int16_t width;
	u_int16_t height;
	u_int16_t depth;
};

#define ICON_RELOAD	205
#define ICON_DOWNLOAD	210
#define ICON_UPLOAD	211
#define ICON_FILE	400
#define ICON_FOLDER	401
#define ICON_FOLDER_IN	421
#define ICON_FILE_HTft	402
#define ICON_FILE_SIT	403
#define ICON_FILE_TEXT	404
#define ICON_FILE_IMAGE	406
#define ICON_FILE_APPL	407
#define ICON_FILE_HTLC	408
#define ICON_FILE_SITP	409
#define ICON_FILE_alis	422
#define ICON_FILE_DISK	423
#define ICON_FILE_NOTE	424
#define ICON_FILE_MOOV	425
#define ICON_FILE_ZIP	426
#define ICON_INFO	215
#define ICON_PREVIEW	217
#define ICON_TRASH	212
#define ICON_MSG	206
#define ICON_KICK	412
#define ICON_BAN	2003
#define ICON_NUKE	2003
#define ICON_CHAT	415
#define ICON_STOP	213
#define ICON_GO		216
#define ICON_NEWS	413
#define ICON_TNEWS	405
#define ICON_CONNECT	411
#define ICON_USER	414
#define ICON_YELLOWUSER	417
#define ICON_BLACKUSER	418
#define ICON_TASKS	416
#define ICON_OPTIONS	419
#define ICON_TRACKER	420
	
extern GtkWidget *icon_button_new(u_int16_t resid, const char *str, GtkWidget *win, GtkTooltips *tt);
extern GtkWidget *icon_pixmap (GtkWidget *widget, u_int16_t icon);
extern struct pixmap_cache *load_icon (GtkWidget *widget, u_int16_t icon, struct ifn *ifn, int browser, int ptype);

#endif /* __ghx_icons_h */
