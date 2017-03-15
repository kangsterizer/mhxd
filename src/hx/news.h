#ifndef __hx_NEWS_H
#define __hx_NEWS_H

struct news_item;
struct news_group;
struct news_folder;

struct date_time {
	u_int16_t base_year;
	u_int16_t pad;
	u_int32_t seconds;
};

struct news_parts {
	int size;
	char *mime_type;
};

struct news_item {
	u_int32_t postid, parentid;
	u_int32_t previd, nextid;
	u_int32_t nextsubid;
	u_int32_t depth;
	char *sender;
	char *subject;
	struct date_time date;
	u_int16_t partcount;
	u_int16_t size;
	struct news_parts *parts;
	struct news_group *group;
	void *node;
};

struct news_post {
	struct news_item *item;
	u_int16_t buflen;
	char *buf;
};

struct news_group {
	u_int32_t post_count;
	char *path;
	struct news_item *posts;
};

struct folder_item {
	char *name;
/*	u_int16_t icon; */
	u_int16_t type;
	u_int16_t count;
};

struct news_folder {
	struct folder_item **entry;
	char *path;
	u_int32_t num_entries;
};

#endif
