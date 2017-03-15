#ifndef HX_NEWS15_H
#define HX_NEWS15_H

struct news_item;
struct news_group;
struct news_folder;

extern void hx_news15_get_post(struct htlc_conn *htlc, struct news_item *item);
extern void hx_news15_cat_list(struct htlc_conn *htlc, char *path);
extern void hx_news15_fldr_list(struct htlc_conn *htlc, char *path);

struct date_time {
  guint16 base_year;
  guint16 pad;
  guint32 seconds;
};

struct news_post {
	char *buf;
	struct news_item *item;
	u_int16_t buflen;
};

struct news_parts {
	int size;
	char *mime_type;
};

struct news_item {
	guint32 postid,parentid;
	char *sender;
	char *subject;
	struct date_time date;
	guint16 partcount;
	guint16 size;
	struct news_parts *parts;
	GtkCTreeNode *node;
	struct news_group *group;
};

struct news_group {
	int post_count;
	char *path;
	struct news_item *posts;
};

struct folder_item {
	char *name;
/*	guint16 icon; */
	int type;
};

struct news_folder {
	struct folder_item **entry;
	char *path;
	guint32 num_entries;
};

#endif
